#!/bin/bash

# Sync WebGPU CTS files to React Native app
# Usage: ./scripts/sync-cts.sh [path-to-cts-repo]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(dirname "$SCRIPT_DIR")"
CTS_DIR="${1:-$APP_DIR/../../../cts}"

# Verify CTS directory exists
if [ ! -d "$CTS_DIR/src/webgpu" ]; then
    echo "Error: CTS directory not found at $CTS_DIR"
    echo "Usage: $0 [path-to-cts-repo]"
    exit 1
fi

echo "Syncing CTS from: $CTS_DIR"
echo "To: $APP_DIR/src/webgpu-cts"

# Clean existing CTS files
echo "Cleaning existing CTS files..."
rm -rf "$APP_DIR/src/webgpu-cts"

# Create directory structure
mkdir -p "$APP_DIR/src/webgpu-cts/src/common/framework"
mkdir -p "$APP_DIR/src/webgpu-cts/src/common/internal"
mkdir -p "$APP_DIR/src/webgpu-cts/src/common/util"
mkdir -p "$APP_DIR/src/webgpu-cts/src/common/runtime/rn/generated"
mkdir -p "$APP_DIR/src/webgpu-cts/src/common/runtime/helper"
mkdir -p "$APP_DIR/src/webgpu-cts/src/webgpu"
mkdir -p "$APP_DIR/src/webgpu-cts/src/external"

# Copy common framework files
echo "Copying common/framework..."
cp -r "$CTS_DIR/src/common/framework/"* "$APP_DIR/src/webgpu-cts/src/common/framework/"

# Copy common internal files
echo "Copying common/internal..."
cp -r "$CTS_DIR/src/common/internal/"* "$APP_DIR/src/webgpu-cts/src/common/internal/"

# Copy common util files
echo "Copying common/util..."
cp -r "$CTS_DIR/src/common/util/"* "$APP_DIR/src/webgpu-cts/src/common/util/"

# Copy React Native runtime
echo "Copying common/runtime/rn..."
cp "$CTS_DIR/src/common/runtime/rn/loader.ts" "$APP_DIR/src/webgpu-cts/src/common/runtime/rn/"
cp "$CTS_DIR/src/common/runtime/rn/runtime.ts" "$APP_DIR/src/webgpu-cts/src/common/runtime/rn/"
cp "$CTS_DIR/src/common/runtime/rn/index.ts" "$APP_DIR/src/webgpu-cts/src/common/runtime/rn/"

# Copy external dependencies (Float16, etc.)
echo "Copying external dependencies..."
cp -r "$CTS_DIR/src/external/"* "$APP_DIR/src/webgpu-cts/src/external/"

# Copy webgpu test specs (excluding web_platform which uses browser-only APIs)
echo "Copying webgpu specs (this may take a moment)..."
rsync -a --exclude='web_platform' "$CTS_DIR/src/webgpu/" "$APP_DIR/src/webgpu-cts/src/webgpu/"

# Remove spec files that import from web_platform (browser-only APIs)
echo "Removing spec files that depend on web_platform..."
find "$APP_DIR/src/webgpu-cts/src/webgpu" -name "*.spec.ts" -exec grep -l "from.*web_platform" {} \; | xargs rm -f 2>/dev/null || true

# Strip .js extensions from imports (Metro bundler doesn't need them)
echo "Stripping .js extensions from imports..."
find "$APP_DIR/src/webgpu-cts/src" -name "*.ts" -type f -exec sed -i '' "s/from '\([^']*\)\.js'/from '\1'/g" {} \;
find "$APP_DIR/src/webgpu-cts/src" -name "*.ts" -type f -exec sed -i '' 's/from "\([^"]*\)\.js"/from "\1"/g' {} \;

# Fix float16 for Metro (rename .js to .impl.js and create .ts wrapper)
echo "Fixing float16 for Metro compatibility..."
if [ -f "$APP_DIR/src/webgpu-cts/src/external/petamoriken/float16/float16.js" ]; then
    mv "$APP_DIR/src/webgpu-cts/src/external/petamoriken/float16/float16.js" \
       "$APP_DIR/src/webgpu-cts/src/external/petamoriken/float16/float16.impl.js"
    cat > "$APP_DIR/src/webgpu-cts/src/external/petamoriken/float16/float16.ts" << 'FLOAT16'
// TypeScript wrapper for Metro compatibility
// @ts-expect-error - importing from JS implementation
export * from './float16.impl.js';
FLOAT16
fi

# Remove DefaultTestFileLoader (uses dynamic imports not supported by Metro)
echo "Removing DefaultTestFileLoader (dynamic imports not supported)..."
sed -i '' '/^export class DefaultTestFileLoader/,/^}$/c\
// DefaultTestFileLoader removed - uses dynamic imports not supported by Metro\
// Use ReactNativeTestFileLoader from ..\/runtime\/rn\/loader instead
' "$APP_DIR/src/webgpu-cts/src/common/internal/file_loader.ts"

# Fix perf_hooks require (React Native has performance globally)
echo "Fixing perf_hooks require..."
sed -i '' "s/typeof performance !== 'undefined' ? performance : require('perf_hooks').performance/performance/" "$APP_DIR/src/webgpu-cts/src/common/util/util.ts"

# Create EventTarget polyfill for React Native
echo "Creating EventTarget polyfill..."
cat > "$APP_DIR/src/webgpu-cts/src/common/util/event-target-polyfill.ts" << 'EVENTTARGET_EOF'
/**
 * Minimal EventTarget polyfill for React Native.
 * Only implements the subset needed by the CTS.
 */

type Listener = (event: any) => void;

interface ListenerEntry {
  listener: Listener | EventListenerObject;
  options?: AddEventListenerOptions;
}

// Polyfill for Event if not available
if (typeof globalThis.Event === 'undefined') {
  (globalThis as any).Event = class Event {
    readonly type: string;
    readonly bubbles: boolean;
    readonly cancelable: boolean;
    readonly composed: boolean;
    readonly timeStamp: number;
    readonly defaultPrevented: boolean = false;
    readonly target: EventTarget | null = null;
    readonly currentTarget: EventTarget | null = null;
    readonly eventPhase: number = 0;
    readonly isTrusted: boolean = false;

    constructor(type: string, eventInitDict?: EventInit) {
      this.type = type;
      this.bubbles = eventInitDict?.bubbles ?? false;
      this.cancelable = eventInitDict?.cancelable ?? false;
      this.composed = eventInitDict?.composed ?? false;
      this.timeStamp = Date.now();
    }

    preventDefault(): void {}
    stopPropagation(): void {}
    stopImmediatePropagation(): void {}
    composedPath(): EventTarget[] { return []; }

    get srcElement(): EventTarget | null { return this.target; }
    get returnValue(): boolean { return !this.defaultPrevented; }
    set returnValue(_value: boolean) {}
    initEvent(_type: string, _bubbles?: boolean, _cancelable?: boolean): void {}

    static readonly NONE = 0;
    static readonly CAPTURING_PHASE = 1;
    static readonly AT_TARGET = 2;
    static readonly BUBBLING_PHASE = 3;

    readonly NONE = 0;
    readonly CAPTURING_PHASE = 1;
    readonly AT_TARGET = 2;
    readonly BUBBLING_PHASE = 3;
  };
}

// Polyfill for MessageEvent if not available
if (typeof globalThis.MessageEvent === 'undefined') {
  (globalThis as any).MessageEvent = class MessageEvent<T = any> {
    readonly type: string;
    readonly data: T;
    readonly origin: string = '';
    readonly lastEventId: string = '';
    readonly source: any = null;
    readonly ports: readonly any[] = [];
    readonly bubbles: boolean = false;
    readonly cancelable: boolean = false;
    readonly defaultPrevented: boolean = false;
    readonly timeStamp: number;

    constructor(type: string, eventInitDict?: { data?: T; bubbles?: boolean; cancelable?: boolean }) {
      this.type = type;
      this.data = eventInitDict?.data as T;
      this.bubbles = eventInitDict?.bubbles ?? false;
      this.cancelable = eventInitDict?.cancelable ?? false;
      this.timeStamp = Date.now();
    }

    preventDefault(): void {}
    stopPropagation(): void {}
    stopImmediatePropagation(): void {}
  };
}

// Polyfill for EventTarget if not available
if (typeof globalThis.EventTarget === 'undefined') {
  (globalThis as any).EventTarget = class EventTarget {
    private listeners: Map<string, ListenerEntry[]> = new Map();

    addEventListener(
      type: string,
      listener: Listener | EventListenerObject | null,
      options?: boolean | AddEventListenerOptions
    ): void {
      if (!listener) return;

      const opts: AddEventListenerOptions | undefined =
        typeof options === 'boolean' ? { capture: options } : options;

      let typeListeners = this.listeners.get(type);
      if (!typeListeners) {
        typeListeners = [];
        this.listeners.set(type, typeListeners);
      }

      const exists = typeListeners.some(
        entry =>
          entry.listener === listener &&
          entry.options?.capture === opts?.capture
      );
      if (!exists) {
        typeListeners.push({ listener, options: opts });
      }
    }

    removeEventListener(
      type: string,
      listener: Listener | EventListenerObject | null,
      options?: boolean | EventListenerOptions
    ): void {
      if (!listener) return;

      const opts: EventListenerOptions | undefined =
        typeof options === 'boolean' ? { capture: options } : options;

      const typeListeners = this.listeners.get(type);
      if (!typeListeners) return;

      const index = typeListeners.findIndex(
        entry =>
          entry.listener === listener &&
          entry.options?.capture === opts?.capture
      );
      if (index !== -1) {
        typeListeners.splice(index, 1);
      }
    }

    dispatchEvent(event: any): boolean {
      const typeListeners = this.listeners.get(event.type);
      if (!typeListeners) return true;

      for (const entry of [...typeListeners]) {
        try {
          if (typeof entry.listener === 'function') {
            entry.listener.call(this, event);
          } else {
            entry.listener.handleEvent(event);
          }
        } catch (e) {
          console.error('Error in event listener:', e);
        }

        if (entry.options?.once) {
          this.removeEventListener(event.type, entry.listener, entry.options);
        }
      }

      return !event.defaultPrevented;
    }
  };
}

export {};
EVENTTARGET_EOF

# Add polyfill import to rn/index.ts
echo "Adding polyfill import to rn/index.ts..."
sed -i '' '1i\
// Polyfill EventTarget and MessageEvent for React Native\
import '\''../../util/event-target-polyfill'\'';\
' "$APP_DIR/src/webgpu-cts/src/common/runtime/rn/index.ts"

# Stub loadMetadataForSuite (uses Node.js fs module not available in RN)
echo "Stubbing loadMetadataForSuite (fs not available)..."
cat > "$APP_DIR/src/webgpu-cts/src/common/framework/metadata.ts" << 'METADATA_EOF'
/** Metadata about tests (that can't be derived at runtime). */
export type TestMetadata = {
  /**
   * Estimated average time-per-subcase, in milliseconds.
   * This is used to determine chunking granularity when exporting to WPT with
   * chunking enabled (like out-wpt/cts-chunked2sec.https.html).
   */
  subcaseMS: number;
};

export type TestMetadataListing = {
  [testQuery: string]: TestMetadata;
};

/**
 * Load metadata for a test suite.
 * Note: In React Native, this always returns null as we don't have filesystem access.
 * This is only used for WPT chunking which isn't needed in RN.
 */
export function loadMetadataForSuite(_suiteDir: string): TestMetadataListing | null {
  return null;
}
METADATA_EOF

# Create React Native compatible options.ts (removes browser window.location dependency)
echo "Creating React Native compatible options.ts..."
cat > "$APP_DIR/src/webgpu-cts/src/common/runtime/helper/options.ts" << 'OPTIONS_EOF'
/**
 * React Native compatible stub for options.ts
 * Browser-specific URL parsing removed.
 */

import { unreachable } from '../../util/util';

/** Runtime modes for running tests in different types of workers. */
export type WorkerMode = 'dedicated' | 'service' | 'shared';

/** Parse a runner option for different worker modes. */
export function optionWorkerMode(
  _opt: string,
  searchParams?: URLSearchParams
): WorkerMode | null {
  if (!searchParams) return null;
  const value = searchParams.get(_opt);
  if (value === null || value === '0') return null;
  if (value === 'service') return 'service';
  if (value === 'shared') return 'shared';
  if (value === '' || value === '1' || value === 'dedicated') return 'dedicated';
  unreachable('invalid worker= option value');
}

export function optionEnabled(opt: string, searchParams?: URLSearchParams): boolean {
  if (!searchParams) return false;
  const val = searchParams.get(opt);
  return val !== null && val !== '0';
}

export function optionString(opt: string, searchParams?: URLSearchParams): string | null {
  if (!searchParams) return null;
  return searchParams.get(opt);
}

export interface CTSOptions {
  worker: WorkerMode | null;
  debug: boolean;
  compatibility: boolean;
  forceFallbackAdapter: boolean;
  enforceDefaultLimits: boolean;
  blockAllFeatures: boolean;
  unrollConstEvalLoops: boolean;
  powerPreference: GPUPowerPreference | null;
  subcasesBetweenAttemptingGC: string;
  casesBetweenReplacingDevice: string;
  logToWebSocket: boolean;
}

export const kDefaultCTSOptions: Readonly<CTSOptions> = {
  worker: null, debug: false, compatibility: false, forceFallbackAdapter: false,
  enforceDefaultLimits: false, blockAllFeatures: false, unrollConstEvalLoops: false,
  powerPreference: null, subcasesBetweenAttemptingGC: '5000',
  casesBetweenReplacingDevice: 'Infinity', logToWebSocket: false,
};

export interface OptionInfo {
  description: string;
  parser?: (key: string, searchParams?: URLSearchParams) => boolean | string | null;
  selectValueDescriptions?: { value: string | null; description: string }[];
}

export type OptionsInfos<Type> = Record<keyof Type, OptionInfo>;

export const kCTSOptionsInfo: OptionsInfos<CTSOptions> = {
  worker: { description: 'run in a worker', parser: optionWorkerMode },
  debug: { description: 'show more info' },
  compatibility: { description: 'request adapters with featureLevel: "compatibility"' },
  forceFallbackAdapter: { description: 'pass forceFallbackAdapter: true to requestAdapter' },
  enforceDefaultLimits: { description: 'force the adapter limits to the default limits' },
  blockAllFeatures: { description: 'block all features on adapter' },
  unrollConstEvalLoops: { description: 'unroll const eval loops in WGSL' },
  powerPreference: { description: 'set default powerPreference', parser: optionString },
  subcasesBetweenAttemptingGC: { description: 'GC interval', parser: optionString },
  casesBetweenReplacingDevice: { description: 'Device replace interval', parser: optionString },
  logToWebSocket: { description: 'log to websocket' },
};

export function camelCaseToSnakeCase(id: string) {
  return id.replace(/(.)([A-Z][a-z]+)/g, '$1_$2').replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase();
}

function getOptionsInfoFromSearchString<Type extends CTSOptions>(
  optionsInfos: OptionsInfos<Type>, searchString: string
): Type {
  const searchParams = new URLSearchParams(searchString);
  const optionValues: Record<string, boolean | string | null> = {};
  for (const [optionName, info] of Object.entries(optionsInfos)) {
    const parser = info.parser || optionEnabled;
    optionValues[optionName] = parser(camelCaseToSnakeCase(optionName), searchParams);
  }
  return optionValues as unknown as Type;
}

export function parseSearchParamLikeWithOptions<Type extends CTSOptions>(
  optionsInfos: OptionsInfos<Type>, query: string
): { queries: string[]; options: Type } {
  const searchString = query.includes('q=') || query.startsWith('?') ? query : \`q=\${query}\`;
  const queries = new URLSearchParams(searchString).getAll('q');
  const options = getOptionsInfoFromSearchString(optionsInfos, searchString);
  return { queries, options };
}

export function parseSearchParamLikeWithCTSOptions(query: string) {
  return parseSearchParamLikeWithOptions(kCTSOptionsInfo, query);
}
OPTIONS_EOF

# Generate all_specs.ts with correct import paths for RN app
echo "Generating all_specs.ts..."

# Find all spec files and generate imports
SPEC_FILES=$(find "$APP_DIR/src/webgpu-cts/src/webgpu" -name "*.spec.ts" | sort)
SPEC_COUNT=$(echo "$SPEC_FILES" | wc -l | tr -d ' ')

echo "Found $SPEC_COUNT spec files"

# Generate the all_specs.ts file
OUTPUT_FILE="$APP_DIR/src/webgpu-cts/src/common/runtime/rn/generated/all_specs.ts"

cat > "$OUTPUT_FILE" << 'HEADER'
/**
 * Auto-generated file - DO NOT EDIT
 * Generated by: scripts/sync-cts.sh
 *
 * This file statically imports all CTS spec files for React Native.
 */

import { AllSpecs, SpecEntry } from '../loader';

HEADER

# Generate imports
INDEX=0
while IFS= read -r SPEC_FILE; do
    # Get path relative to webgpu dir
    REL_PATH="${SPEC_FILE#$APP_DIR/src/webgpu-cts/src/webgpu/}"
    # Remove .spec.ts extension
    PATH_NO_EXT="${REL_PATH%.spec.ts}"
    # Generate import
    echo "import * as spec${INDEX} from '../../../../webgpu/${PATH_NO_EXT}.spec';" >> "$OUTPUT_FILE"
    INDEX=$((INDEX + 1))
done <<< "$SPEC_FILES"

echo "" >> "$OUTPUT_FILE"
echo "const webgpuSpecs: SpecEntry[] = [" >> "$OUTPUT_FILE"

# Generate entries
INDEX=0
while IFS= read -r SPEC_FILE; do
    # Get path relative to webgpu dir
    REL_PATH="${SPEC_FILE#$APP_DIR/src/webgpu-cts/src/webgpu/}"
    # Remove .spec.ts extension
    PATH_NO_EXT="${REL_PATH%.spec.ts}"
    # Split into path parts
    IFS='/' read -ra PARTS <<< "$PATH_NO_EXT"
    # Format as array
    PATH_ARRAY=$(printf "'%s', " "${PARTS[@]}")
    PATH_ARRAY="${PATH_ARRAY%, }"
    echo "  { path: [${PATH_ARRAY}], spec: spec${INDEX} }," >> "$OUTPUT_FILE"
    INDEX=$((INDEX + 1))
done <<< "$SPEC_FILES"

cat >> "$OUTPUT_FILE" << 'FOOTER'
];

export const allSpecs: AllSpecs = new Map([
  ['webgpu', webgpuSpecs],
]);

export default allSpecs;
FOOTER

echo ""
echo "Done! CTS synced to $APP_DIR/src/webgpu-cts"
echo ""
echo "Files copied:"
echo "  - src/common/framework/ (test fixtures, params)"
echo "  - src/common/internal/ (tree, query, logging)"
echo "  - src/common/util/ (utilities)"
echo "  - src/common/runtime/rn/ (React Native runtime)"
echo "  - src/webgpu/ ($SPEC_COUNT spec files)"
echo ""
echo "Generated:"
echo "  - src/common/runtime/rn/generated/all_specs.ts"
