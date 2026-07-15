/* eslint-disable @typescript-eslint/no-explicit-any */

import fs from "fs";
import path from "path";

import puppeteer from "puppeteer";
import { PNG } from "pngjs";
import pixelmatch from "pixelmatch";
import { mat4, vec3, mat3 } from "wgpu-matrix";
import type { Server, WebSocket } from "ws";
import type { Browser, Page } from "puppeteer";

import type { GPUOffscreenCanvas } from "../Offscreen";

import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./components/triangle";
import { DEBUG, NODE_WEBGPU, REFERENCE } from "./config";

jest.setTimeout(180 * 1000);

type TestOS = "ios" | "android" | "web" | "node";

declare global {
  var testServer: Server;
  var testClient: WebSocket;
  var testOS: TestOS;
  var testArch: "paper" | "fabric";
}

interface GPUTestingContext {
  gpu: GPU;
  device: GPUDevice;
  shaders: {
    triangleVertWGSL: string;
    redFragWGSL: string;
  };
  urls: {
    fTexture: string;
  };
  assets: {
    cubeVertexArray: Float32Array;
    di3D: ImageData;
    moon: ImageData;
    saturn: ImageData;
  };
  ctx: GPUCanvasContext;
  canvas: GPUOffscreenCanvas;
  mat4: typeof mat4;
  vec3: typeof vec3;
  mat3: typeof mat3;
}

type Ctx = Record<string, unknown>;

type JSONValue =
  | { [key: string]: JSONValue }
  | JSONValue[]
  | number
  | string
  | boolean
  | null;

interface TestingClient {
  eval<C = Ctx, R = JSONValue>(
    fn: (ctx: GPUTestingContext & C) => R | Promise<R>,
    ctx?: C,
  ): Promise<R>;
  OS: TestOS;
  arch: "paper" | "fabric";
  init(): Promise<void>;
  dispose(): Promise<void>;
}

export let client: TestingClient;

beforeAll(async () => {
  if (REFERENCE) {
    client = new ReferenceTestingClient();
  } else if (NODE_WEBGPU) {
    client = new NodeTestingClient();
  } else {
    client = new RemoteTestingClient();
  }
  await client.init();
});

afterAll(async () => {
  await client.dispose();
});

class RemoteTestingClient implements TestingClient {
  readonly OS = global.testOS;
  readonly arch = global.testArch;

  eval<C = Ctx, R = JSONValue>(
    fn: (ctx: GPUTestingContext & C) => R | Promise<R>,
    context?: C,
  ): Promise<R> {
    const ctx = this.prepareContext(context ?? {});
    const body = { code: fn.toString(), ctx };
    return this.handleResponse<R>(JSON.stringify(body));
  }

  private handleResponse<R>(body: string): Promise<R> {
    // Guard against an eval that never replies (e.g. the device threw and could
    // not post a result back). Without this the host would await forever and a
    // single failing case would stall the whole serial suite.
    const EVAL_TIMEOUT_MS = 30 * 1000;
    return new Promise((resolve, reject) => {
      const onMessage = (raw: Buffer) => {
        clearTimeout(timeout);
        const response = JSON.parse(raw.toString());
        // The device reports a thrown error as { $$error: message } so the
        // matching test can `.rejects` instead of hanging on a missing reply.
        if (
          response !== null &&
          typeof response === "object" &&
          "$$error" in response
        ) {
          reject(new Error(String((response as { $$error: unknown }).$$error)));
        } else {
          resolve(response);
        }
      };
      const timeout = setTimeout(() => {
        this.client.off("message", onMessage);
        reject(
          new Error(
            `eval timed out after ${EVAL_TIMEOUT_MS}ms without a response from the device`,
          ),
        );
      }, EVAL_TIMEOUT_MS);
      this.client.once("message", onMessage);
      this.client.send(body);
    });
  }

  private get client() {
    if (global.testClient === null) {
      throw new Error("Client is not connected. Did you call init?");
    }
    return global.testClient!;
  }

  private prepareContext<C extends Ctx>(context?: C): C {
    const ctx: any = {};
    if (context) {
      for (const [key, value] of Object.entries(context)) {
        ctx[key] = value;
      }
    }
    return ctx;
  }
  async init() {}
  async dispose() {}
}

class ReferenceTestingClient implements TestingClient {
  readonly OS = "web";
  readonly arch = "paper";

  private browser: Browser | null = null;
  private page: Page | null = null;

  async eval<C = Ctx, R = JSONValue>(
    fn: (ctx: GPUTestingContext & C) => R | Promise<R>,
    ctx?: C,
  ): Promise<R> {
    if (!this.page) {
      throw new Error("RemoteSurface not initialized");
    }
    const fTexturePath = path.join(
      __dirname,
      "../../../../apps/example/src/assets/f.png",
    );
    const fTextureData = fs.readFileSync(fTexturePath);
    const fTextureBase64 = `data:image/png;base64,${fTextureData.toString("base64")}`;
    const source = `(async function Main(){
    var global = window;  
    const r = () => {${fs.readFileSync(path.join(__dirname, "../../../../node_modules/wgpu-matrix/dist/3.x/wgpu-matrix.js"), "utf8")} };
      r();
      const { mat4, vec3, mat3 } = window.wgpuMatrix;
      const { device, adapter, gpu, cubeVertexArray, triangleVertWGSL, redFragWGSL, di3D, saturn, moon } = window;
      class DrawingContext {
        constructor(device, width, height) {
            this.device = device;
            this.width = width;
            this.height = height;
            const bytesPerRow = this.width * 4;
            this.texture = device.createTexture({
                size: [width, height],
                format: gpu.getPreferredCanvasFormat(),
                usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
            });
            this.buffer = device.createBuffer({
                size: bytesPerRow * this.height,
                usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
            });
        }
        getCurrentTexture() {
            return this.texture;
        }

        get canvas() {
          return {
            width: this.width,
            height: this.height,
          };
        }

        getImageData() {
            const commandEncoder = this.device.createCommandEncoder();
            const bytesPerRow = this.width * 4;
            commandEncoder.copyTextureToBuffer({ texture: this.texture }, { buffer: this.buffer, bytesPerRow }, [this.width, this.height]);
            this.device.queue.submit([commandEncoder.finish()]);

            return this.buffer.mapAsync(GPUMapMode.READ).then(() => {
              const arrayBuffer = this.buffer.getMappedRange();
              const uint8Array = new Uint8Array(arrayBuffer);
              const data = Array.from(uint8Array);
              this.buffer.unmap();
              return {
                data,
                width: this.width,
                height: this.height,
                format: gpu.getPreferredCanvasFormat(),
              };
            });
        }
    } 

      const ctx = new DrawingContext(device, 1024, 1024);
      return (${fn.toString()})({
        device, adapter, gpu,
        urls: {
          fTexture: "${fTextureBase64}"
        },
        assets: {
          cubeVertexArray,
          di3D,
          moon,
          saturn,
        },
        shaders: {
          triangleVertWGSL,
          redFragWGSL,
        },
        ctx,
        canvas: {
          getImageData: ctx.getImageData.bind(ctx),
          width: ctx.width,
          height: ctx.height,
        },
        mat4,
        vec3,
        mat3,
        ...${JSON.stringify(ctx || {})}
      });
    })();`;
    const data = await this.page.evaluate(source);
    return data as R;
  }

  async init() {
    const browser = await puppeteer.launch({
      headless: !DEBUG,
      args: ["--enable-unsafe-webgpu"],
    });
    const page = await browser.newPage();
    page.on("console", (msg) => console.log(msg.text()));
    page.on("pageerror", (error) => {
      console.error(error.message);
    });
    await page
      .goto("chrome://gpu", {
        waitUntil: "networkidle0",
        timeout: 20 * 60 * 1000,
      })
      .catch((e) => console.log(e));
    await page.waitForNetworkIdle();
    const di3D = decodeImage(
      path.join(__dirname, "../../../../apps/example/src/assets/Di-3d.png"),
    );
    const moon = decodeImage(
      path.join(__dirname, "../../../../apps/example/src/assets/moon.png"),
    );
    const saturn = decodeImage(
      path.join(__dirname, "../../../../apps/example/src/assets/saturn.png"),
    );
    await page.evaluate(
      `
(async () => {
  window.gpu = navigator.gpu;
  if (!gpu) {
    const canvas = document.createElement('canvas');
    const gl = canvas.getContext('webgl');
    throw new Error("WebGPU is not available. WebGL: " + !!(gl && gl instanceof WebGLRenderingContext));
  }
  window.adapter = await gpu.requestAdapter();
  if (!adapter) {
    throw new Error("No adapter");
  }
  window.RNWebGPU = {
    DecodeToUTF8: (data) => {
      return new TextDecoder().decode(data);
    }
  };
  window.device = await adapter.requestDevice();
  window.cubeVertexArray = new Float32Array(${JSON.stringify(Array.from(cubeVertexArray))});
  window.triangleVertWGSL = \`${triangleVertWGSL}\`;
  window.redFragWGSL = \`${redFragWGSL}\`;
  const rawDi3D = ${JSON.stringify(di3D)};
  window.di3D = new ImageData(
    new Uint8ClampedArray(rawDi3D.data),
    rawDi3D.width,
    rawDi3D.height
  );
  const rawMoon = ${JSON.stringify(moon)};
  window.moon = new ImageData(
    new Uint8ClampedArray(rawMoon.data),
    rawMoon.width,
    rawMoon.height
  );
  const rawSaturn = ${JSON.stringify(saturn)};
  window.saturn = new ImageData(
    new Uint8ClampedArray(rawSaturn.data),
    rawSaturn.width,
    rawSaturn.height
  );
})();
      `,
    );
    this.browser = browser;
    this.page = page;
  }
  async dispose() {
    if (this.browser && !DEBUG) {
      this.browser.close();
    }
  }
}

// Offscreen render target mirroring the DrawingContext the reference client
// injects into the Chrome page: getCurrentTexture() returns a plain texture
// and getImageData() reads it back through a mapped buffer.
class NodeDrawingContext {
  private texture: GPUTexture;
  private buffer: GPUBuffer;

  constructor(
    private device: GPUDevice,
    private format: GPUTextureFormat,
    readonly width: number,
    readonly height: number,
  ) {
    this.texture = device.createTexture({
      size: [width, height],
      format,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });
    this.buffer = device.createBuffer({
      size: width * 4 * height,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });
  }

  get canvas() {
    return { width: this.width, height: this.height };
  }

  getCurrentTexture() {
    return this.texture;
  }

  async getImageData(): Promise<BitmapData> {
    const commandEncoder = this.device.createCommandEncoder();
    const bytesPerRow = this.width * 4;
    commandEncoder.copyTextureToBuffer(
      { texture: this.texture },
      { buffer: this.buffer, bytesPerRow },
      [this.width, this.height],
    );
    this.device.queue.submit([commandEncoder.finish()]);
    await this.buffer.mapAsync(GPUMapMode.READ);
    const data = Array.from(new Uint8Array(this.buffer.getMappedRange()));
    this.buffer.unmap();
    return {
      data,
      width: this.width,
      height: this.height,
      format: this.format,
    };
  }
}

class NodeTestingClient implements TestingClient {
  readonly OS = "node" as const;
  readonly arch = "paper" as const;

  private gpu: GPU | null = null;
  private device: GPUDevice | null = null;

  async eval<C = Ctx, R = JSONValue>(
    fn: (ctx: GPUTestingContext & C) => R | Promise<R>,
    ctx?: C,
  ): Promise<R> {
    if (!this.gpu || !this.device) {
      throw new Error("NodeTestingClient not initialized. Did you call init?");
    }
    const { gpu } = this;
    const { device } = this;
    const drawingContext = new NodeDrawingContext(
      device,
      gpu.getPreferredCanvasFormat(),
      1024,
      1024,
    );
    const fTexturePath = path.join(
      __dirname,
      "../../../../apps/example/src/assets/f.png",
    );
    const fTextureBase64 = `data:image/png;base64,${fs.readFileSync(fTexturePath).toString("base64")}`;
    const toImageData = (relPath: string) => {
      const bitmap = decodeImage(relPath);
      // Node has no ImageData constructor; a structurally compatible object
      // is enough for writeTexture-based tests.
      return {
        data: new Uint8ClampedArray(bitmap.data),
        width: bitmap.width,
        height: bitmap.height,
        colorSpace: "srgb",
      } as ImageData;
    };
    const base = {
      gpu,
      device,
      shaders: { triangleVertWGSL, redFragWGSL },
      urls: { fTexture: fTextureBase64 },
      assets: {
        cubeVertexArray,
        di3D: toImageData("../../../../apps/example/src/assets/Di-3d.png"),
        moon: toImageData("../../../../apps/example/src/assets/moon.png"),
        saturn: toImageData("../../../../apps/example/src/assets/saturn.png"),
      },
      ctx: drawingContext as unknown as GPUCanvasContext,
      canvas: {
        width: drawingContext.width,
        height: drawingContext.height,
        getImageData: () => drawingContext.getImageData(),
      } as unknown as GPUOffscreenCanvas,
      mat4,
      vec3,
      mat3,
    };
    return fn(Object.assign(base, ctx) as GPUTestingContext & C);
  }

  async init() {
    // Load the prebuilt dawn.node binary directly: the package's index.js is
    // ESM-only (import.meta), which jest's CommonJS transform cannot load.
    const arch = process.platform === "darwin" ? "universal" : process.arch;
    const { create, globals } = require(
      `webgpu/dist/${process.platform}-${arch}.dawn.node`,
    ) as {
      create: (flags: string[]) => GPU;
      globals: Record<string, unknown>;
    };
    // Expose GPUBufferUsage, GPUTextureUsage, GPUMapMode, ... to test code.
    Object.assign(globalThis, globals);
    (globalThis as Record<string, unknown>).RNWebGPU = {
      DecodeToUTF8: (data: NodeJS.ArrayBufferView) =>
        new TextDecoder().decode(data),
    };
    // Same instance-level toggles as the native runtime (see GPU.cpp).
    this.gpu = create([
      "enable-dawn-features=allow_unsafe_apis,expose_wgsl_experimental_features",
    ]);
    const adapter = await this.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("dawn.node returned no WebGPU adapter");
    }
    this.device = await adapter.requestDevice();
    this.installWebPolyfills(this.device);
  }

  // dawn.node implements the core WebGPU API but none of the web-platform
  // image machinery, so provide the minimal pieces the specs rely on.
  private installWebPolyfills(device: GPUDevice) {
    const decodePng = (bytes: Uint8Array) => {
      const png = PNG.sync.read(
        Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength),
      );
      return {
        data: new Uint8ClampedArray(png.data),
        width: png.width,
        height: png.height,
        close() {},
      };
    };
    (globalThis as Record<string, unknown>).createImageBitmap = async (
      source: unknown,
    ) => {
      if (source instanceof ArrayBuffer) {
        return decodePng(new Uint8Array(source));
      }
      if (ArrayBuffer.isView(source)) {
        return decodePng(
          new Uint8Array(source.buffer, source.byteOffset, source.byteLength),
        );
      }
      if (typeof Blob !== "undefined" && source instanceof Blob) {
        return decodePng(new Uint8Array(await source.arrayBuffer()));
      }
      if (
        source !== null &&
        typeof source === "object" &&
        "data" in source &&
        "width" in source
      ) {
        // Already an ImageData-like object (e.g. one of the test assets).
        return source;
      }
      throw new Error("createImageBitmap polyfill: unsupported source");
    };
    // copyExternalImageToTexture expects an ImageBitmap; route the raw RGBA
    // bytes of our ImageData-like sources through writeTexture instead.
    Object.defineProperty(device.queue, "copyExternalImageToTexture", {
      configurable: true,
      value: (
        source: {
          source: { data: Uint8ClampedArray; width: number; height: number };
        },
        destination: GPUTexelCopyTextureInfo,
        copySize: GPUExtent3DStrict,
      ) => {
        const { data, width } = source.source;
        device.queue.writeTexture(
          destination,
          new Uint8Array(
            data.buffer as ArrayBuffer,
            data.byteOffset,
            data.byteLength,
          ),
          { bytesPerRow: width * 4 },
          copySize,
        );
      },
    });
  }

  async dispose() {
    this.device?.destroy();
    // Drop the reference to the dawn.node instance so node can exit.
    this.device = null;
    this.gpu = null;
  }
}

interface BitmapData {
  data: number[];
  width: number;
  height: number;
  format: string;
}

export const encodeImage = (bitmap: BitmapData) => {
  const { width, height, format } = bitmap;
  let data = new Uint8Array(bitmap.data);
  // Convert BGRA to RGBA if necessary
  if (format === "bgra8unorm") {
    data = new Uint8Array(bitmap.data.length);
    for (let i = 0; i < bitmap.data.length; i += 4) {
      data[i] = bitmap.data[i + 2]; // R
      data[i + 1] = bitmap.data[i + 1]; // G
      data[i + 2] = bitmap.data[i]; // B
      data[i + 3] = bitmap.data[i + 3]; // A
    }
  } else if (format !== "rgba8unorm") {
    throw new Error(`Unsupported format ${format}`);
  }
  // Create a new PNG
  const png = new PNG({
    width: width,
    height: height,
    filterType: -1,
  });
  png.data = Buffer.from(data);
  return png;
};

interface CheckImageOptions {
  maxPixelDiff?: number;
  threshold?: number;
  overwrite?: boolean;
  mute?: boolean;
  shouldFail?: boolean;
}

// On Github Action, the image decoding is slightly different
// all tests that show the oslo.jpg have small differences but look ok
const defaultCheckImageOptions = {
  maxPixelDiff: 200,
  threshold: 0.1,
  overwrite: false,
  mute: false,
  shouldFail: false,
};

export const checkImage = (
  toTest: PNG,
  relPath: string,
  opts?: CheckImageOptions,
) => {
  const options = { ...defaultCheckImageOptions, ...opts };
  const { overwrite, threshold, mute, maxPixelDiff, shouldFail } = options;
  const p = path.resolve(__dirname, relPath);
  if (fs.existsSync(p) && !overwrite) {
    const ref = fs.readFileSync(p);
    const baseline = PNG.sync.read(ref);
    const diffImage = new PNG({
      width: baseline.width,
      height: baseline.height,
    });
    if (baseline.width !== toTest.width || baseline.height !== toTest.height) {
      throw new Error(
        `Image sizes don't match: ${baseline.width}x${baseline.height} vs ${toTest.width}x${toTest.height}`,
      );
    }
    const diffPixelsCount = pixelmatch(
      baseline.data,
      toTest.data,
      diffImage.data,
      baseline.width,
      baseline.height,
      { threshold },
    );
    if (!mute) {
      if (diffPixelsCount > maxPixelDiff && !shouldFail) {
        console.log(`${p} didn't match`);
        fs.writeFileSync(`${p}.test.png`, PNG.sync.write(toTest));
        fs.writeFileSync(`${p}-diff-test.png`, PNG.sync.write(diffImage));
      }
      if (shouldFail) {
        expect(diffPixelsCount).not.toBeLessThanOrEqual(maxPixelDiff);
      } else {
        expect(diffPixelsCount).toBeLessThanOrEqual(maxPixelDiff);
      }
    }
    return diffPixelsCount;
  } else {
    const buffer = PNG.sync.write(toTest);
    fs.writeFileSync(p, buffer);
  }
  return 0;
};

export const itSkipsOnWeb = (name: string, fn: () => Promise<void>) => {
  it(name, async () => {
    if (client.OS === "web") {
      return;
    }
    await fn();
  });
};

export const decodeImage = (relPath: string): BitmapData => {
  const p = path.resolve(__dirname, relPath);
  const data = fs.readFileSync(p);
  const png = PNG.sync.read(data);

  const bitmap: BitmapData = {
    data: Array.from(png.data),
    width: png.width,
    height: png.height,
    format: "rgba8unorm",
  };

  return bitmap;
};
