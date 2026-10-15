// Web counterpart of ./installNative.ts, so the package entry can import
// `./main/installNative` on every platform without reaching the TurboModule
// here. The browser provides the WebGPU globals (navigator.gpu, the flag
// constants); the polyfill provides the RNWebGPU global that Canvas relies on.
import "../WebPolyfillGPUModule";
