/**
 * WGSL shader preprocessor implementation
 */
import { fetchInclude, parseUint32, WGSLError } from "./utils";

// Regular expressions for preprocessing
const RE_COMMENT = /(\/\/.*|\/\*[\s\S]*?\*\/)/g;
const RE_WORD = /[a-zA-Z_][a-zA-Z0-9_]*/g;

const STRING_MAX_LEN = 20;

/**
 * Maps the processed shader source to original line numbers
 */
export class SourceMap {
  extensions = "";
  source = "";
  map: number[] = [0];
  workgroupCount = new Map<string, [number, number, number]>();
  dispatchOnce = new Map<string, boolean>();
  dispatchCount = new Map<string, number>();
  // assertMap: number[] = [];
  // userData = new Map<string, Uint32Array>([['_dummy', new Uint32Array([0])]]);

  /**
   * Add a line to the source map
   */
  pushLine(line: string, lineNumber: number) {
    this.source += line + "\n";
    this.map.push(lineNumber);
  }
}

/**
 * Handles WGSL preprocessing including includes, defines, etc.
 */
export class Preprocessor {
  private defines: Map<string, string>;
  private source: SourceMap;
  private storageCount: number;
  // private assertCount: number;
  private specialStrings: boolean;

  constructor(defines: Map<string, string>) {
    this.defines = new Map(defines);
    this.defines.set("STRING_MAX_LEN", STRING_MAX_LEN.toString());
    this.source = new SourceMap();
    this.storageCount = 0;
    // this.assertCount = 0;
    this.specialStrings = false;
  }

  /**
   * Strip comments from WGSL source
   */
  static stripComments(source: string): string {
    return source.replace(RE_COMMENT, "");
  }

  /**
   * Substitute defines in source text
   */
  private substDefines(source: string): string {
    return source.replace(RE_WORD, (match) => {
      return this.defines.get(match) ?? match;
    });
  }

  /**
   * Process a single line of shader source
   */
  private async processLine(lineOrig: string, lineNum: number): Promise<void> {
    let line = this.substDefines(lineOrig);

    // Handle enable directives
    if (line.trimStart().startsWith("enable")) {
      line = line.replace(RE_COMMENT, "");
      this.source.extensions += line + "\n";
      return;
    }

    // Handle preprocessor directives
    if (line.trimStart().startsWith("#")) {
      line = line.replace(RE_COMMENT, "");
      const tokens = line.trim().split(" ");
      const directive = tokens[0];

      switch (directive) {
        case "#include":
          await this.handleInclude(tokens, lineNum);
          break;

        case "#workgroup_count":
          this.handleWorkgroupCount(tokens, lineNum);
          break;

        case "#dispatch_once":
          this.handleDispatchOnce(tokens);
          break;

        case "#dispatch_count":
          this.handleDispatchCount(tokens, lineNum);
          break;

        case "#define":
          this.handleDefine(lineOrig, tokens, lineNum);
          break;

        case "#storage":
          this.handleStorage(tokens, lineNum);
          break;

        // case '#assert':
        //     this.handleAssert(tokens, lineNum);
        //     break;

        // case '#data':
        //     this.handleData(tokens, lineNum);
        //     break;

        default:
          throw new WGSLError("Unrecognized preprocessor directive", lineNum);
      }
      return;
    }

    // Handle string literals if enabled
    if (this.specialStrings) {
      let error: WGSLError | null = null;
      line = line.replace(/"((?:[^\\"]|\\.)*)"/g, (match) => {
        try {
          const unescaped = JSON.parse(match) as string;
          const chars = Array.from(unescaped).map((c) => c.charCodeAt(0));

          if (chars.length > STRING_MAX_LEN) {
            error = new WGSLError(
              `String literals cannot be longer than ${STRING_MAX_LEN} characters`,
              lineNum
            );
            return match;
          }

          // Pad array to fixed length
          const len = chars.length;
          while (chars.length < STRING_MAX_LEN) {
            chars.push(0);
          }

          return `String(${len}, array<u32,${STRING_MAX_LEN}>(${chars
            .map((c) => `0x${c.toString(16).padStart(4, "0")}`)
            .join(", ")}))`;
        } catch (e) {
          console.error(e);
          return match;
        }
      });

      if (error) {
        throw error;
      }
    }

    this.source.pushLine(line, lineNum);
  }

  /**
   * Handle #include directive
   */
  private async handleInclude(
    tokens: string[],
    lineNum: number
  ): Promise<void> {
    if (tokens.length !== 2) {
      throw new WGSLError("Invalid #include syntax", lineNum);
    }

    const nameMatcher = tokens[1].match(/"(.*)"/) || tokens[1].match(/<(.*)>/);
    if (!nameMatcher) {
      throw new WGSLError(
        "Path must be enclosed in quotes or chevrons",
        lineNum
      );
    }

    const name = nameMatcher[1];
    if (/<.*>/.test(tokens[1]) && name === "string") {
      this.specialStrings = true;
    }

    const includePath = /<.*>/.test(tokens[1]) ? `std/${name}` : name;
    const includeContent = await fetchInclude(includePath);

    if (!includeContent) {
      throw new WGSLError(`Cannot find include ${tokens[1]}`, lineNum);
    }

    for (const includeLine of includeContent.split("\n")) {
      await this.processLine(includeLine, lineNum);
    }
  }

  /**
   * Handle #workgroup_count directive
   */
  private handleWorkgroupCount(tokens: string[], lineNum: number): void {
    if (tokens.length !== 5) {
      throw new WGSLError("Invalid #workgroup_count syntax", lineNum);
    }

    const [, name, x, y, z] = tokens;
    this.source.workgroupCount.set(name, [
      parseUint32(x, lineNum),
      parseUint32(y, lineNum),
      parseUint32(z, lineNum),
    ]);
  }

  /**
   * Handle #dispatch_once directive
   */
  private handleDispatchOnce(tokens: string[]): void {
    const [, name] = tokens;
    this.source.dispatchOnce.set(name, true);
  }

  /**
   * Handle #dispatch_count directive
   */
  private handleDispatchCount(tokens: string[], lineNum: number): void {
    if (tokens.length !== 3) {
      throw new WGSLError("Invalid #dispatch_count syntax", lineNum);
    }

    const [, name, count] = tokens;
    this.source.dispatchCount.set(name, parseUint32(count, lineNum));
  }

  /**
   * Handle #define directive
   */
  private handleDefine(
    lineOrig: string,
    tokens: string[],
    lineNum: number
  ): void {
    const name = lineOrig.trim().split(" ")[1];
    if (!name) {
      throw new WGSLError("Invalid #define syntax", lineNum);
    }

    const value = tokens.slice(2).join(" ");
    if (this.defines.has(name)) {
      throw new WGSLError(`Cannot redefine ${name}`, lineNum);
    }

    this.defines.set(name, value);
  }

  /**
   * Handle #storage directive
   */
  private handleStorage(tokens: string[], lineNum: number): void {
    if (this.storageCount >= 2) {
      throw new WGSLError(
        "Only two storage buffers are currently supported",
        lineNum
      );
    }

    const [, name, ...types] = tokens;
    const type = types.join(" ");
    this.source.pushLine(
      `@group(0) @binding(${this.storageCount}) var<storage,read_write> ${name}: ${type};`,
      lineNum
    );
    this.storageCount++;
  }

  /*
    private handleAssert(tokens: string[], lineNum: number): void {
        if (this.assertCount >= NUM_ASSERT_COUNTERS) {
            throw new WGSLError(
                `A maximum of ${NUM_ASSERT_COUNTERS} assertions are currently supported`,
                lineNum
            );
        }

        const predicate = tokens.slice(1).join(' ');
        this.source.pushLine(`assert(${this.assertCount}, ${predicate});`, lineNum);
        this.source.assertMap.push(lineNum);
        this.assertCount++;
    }

    private handleData(tokens: string[], lineNum: number): void {
        if (tokens.length < 4 || tokens[2] !== 'u32') {
            throw new WGSLError('Invalid #data syntax', lineNum);
        }

        const name = tokens[1];
        const dataStr = tokens.slice(3).join('');
        const data = new Uint32Array(dataStr.split(',').map(s => parseUint32(s, lineNum)));

        const existing = this.source.userData.get(name);
        if (existing) {
            // Append to existing data
            const combined = new Uint32Array(existing.length + data.length);
            combined.set(existing);
            combined.set(data, existing.length);
            this.source.userData.set(name, combined);
        } else {
            this.source.userData.set(name, data);
        }
    }
    */

  /**
   * Process complete shader source
   */
  async preprocess(shader: string): Promise<SourceMap> {
    const lines = shader.split("\n");
    for (let i = 0; i < lines.length; i++) {
      await this.processLine(lines[i], i + 1);
    }
    return this.source;
  }
}
