# WebGPU CTS for React Native

This directory contains the [WebGPU Conformance Test Suite (CTS)](https://github.com/nicebuild-org/nicebuild/webgpu-cts) adapted for React Native.

## Overview

The WebGPU CTS is designed for browser and Node.js environments with dynamic imports. React Native (via Metro bundler) doesn't support dynamic imports, so we use a build step to generate static imports for all spec files.

## Syncing the CTS

To update the CTS files from the upstream repository:

```bash
# Clone the CTS repo (if not already)
git clone https://github.com/nicebuild-org/nicebuild/webgpu-cts.git /path/to/cts

# Run the sync script
./scripts/sync-cts.sh /path/to/cts
```

The sync script:
1. Copies the necessary CTS source files
2. Excludes `web_platform` tests (browser-only APIs)
3. Strips `.js` extensions from imports (Metro compatibility)
4. Creates polyfills for missing Web APIs (Event, EventTarget, MessageEvent)
5. Stubs Node.js-specific code (fs, perf_hooks)
6. Generates `all_specs.ts` with static imports for all spec files

## Running Tests

```tsx
import { CTSRunner } from './webgpu-cts/src/common/runtime/rn';
import { allSpecs } from './webgpu-cts/src/common/runtime/rn/generated/all_specs';

// Create a runner
const runner = new CTSRunner(allSpecs, {
  debug: false,
  compatibility: false,
});

// Run tests matching a query
const { summary, results } = await runner.runTests('webgpu:api,operation,adapter,*', {
  onTestStart: (name, index, total) => {
    console.log(`Running ${index + 1}/${total}: ${name}`);
  },
  onTestComplete: (result) => {
    console.log(`${result.status}: ${result.name}`);
  },
});

console.log(`Passed: ${summary.passed}, Failed: ${summary.failed}`);
```

## Query Syntax

The CTS uses a hierarchical query syntax:

- `webgpu:*` - All WebGPU tests
- `webgpu:api,*` - All API tests
- `webgpu:api,operation,*` - All operation tests
- `webgpu:api,operation,adapter,*` - All adapter tests
- `webgpu:api,operation,adapter,info:*` - Specific test file

## Polyfills

The following Web APIs are polyfilled for React Native:

- `Event` - Base event class
- `EventTarget` - Event dispatching
- `MessageEvent` - Used for progress events

These are automatically loaded via `event-target-polyfill.ts`.

## Modifications from Upstream

The sync script makes these modifications:

| File | Change |
|------|--------|
| `common/framework/metadata.ts` | Stub `loadMetadataForSuite` (no fs access) |
| `common/util/util.ts` | Remove `perf_hooks` require |
| `common/internal/file_loader.ts` | Remove `DefaultTestFileLoader` |
| `common/runtime/helper/options.ts` | Remove `window.location` dependency |
| `external/petamoriken/float16/` | Wrap JS file for Metro |

## Architecture

```
src/webgpu-cts/
├── src/
│   ├── common/
│   │   ├── framework/      # Test fixtures, params
│   │   ├── internal/       # Query parsing, tree building
│   │   ├── runtime/
│   │   │   ├── rn/         # React Native runtime
│   │   │   │   ├── loader.ts       # ReactNativeTestFileLoader
│   │   │   │   ├── runtime.ts      # CTSRunner
│   │   │   │   ├── index.ts        # Public exports
│   │   │   │   └── generated/
│   │   │   │       └── all_specs.ts  # Static imports (generated)
│   │   │   └── helper/
│   │   │       └── options.ts      # RN-compatible options
│   │   └── util/
│   │       └── event-target-polyfill.ts  # Web API polyfills
│   ├── external/           # Third-party (float16)
│   └── webgpu/             # Test specs
```

## Contributing

When contributing CTS changes upstream, note that the React Native runtime files are in:
- `src/common/runtime/rn/loader.ts`
- `src/common/runtime/rn/runtime.ts`
- `src/common/runtime/rn/index.ts`
