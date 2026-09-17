/* globals jest */

// `react-native-webgpu` wraps a native TurboModule (`NativeWebGPUModule.ts`
// calls `TurboModuleRegistry.getEnforcing("WebGPUModule")` at import time),
// which throws under Jest since there is no native binary to register
// against. Swap the whole package for a JS-only stand-in (see `src/mock.ts`)
// so screens built around `<Canvas>` / `importDevice` / etc. can still be
// imported and rendered in tests.
jest.mock("react-native-webgpu", () =>
  require("react-native-webgpu/lib/module/mock"),
);
