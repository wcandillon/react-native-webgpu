import { NitroModules } from "react-native-nitro-modules";

const CONSTRUCTOR_NAMES = [
  "GPU",
  "GPUAdapter",
  "GPUAdapterInfo",
  "GPUBindGroup",
  "GPUBindGroupLayout",
  "GPUBuffer",
  "GPUCanvasContext",
  "GPUCommandBuffer",
  "GPUCommandEncoder",
  "GPUCompilationInfo",
  "GPUCompilationMessage",
  "GPUComputePassEncoder",
  "GPUComputePipeline",
  "GPUDevice",
  "GPUDeviceLostInfo",
  "GPUError",
  "GPUExternalTexture",
  "GPUInternalError",
  "GPUOutOfMemoryError",
  "GPUValidationError",
  "GPUUncapturedErrorEvent",
  "GPUPipelineLayout",
  "GPUQuerySet",
  "GPUQueue",
  "GPURenderBundle",
  "GPURenderBundleEncoder",
  "GPURenderPassEncoder",
  "GPURenderPipeline",
  "GPUSampler",
  "GPUSharedFence",
  "GPUSharedTextureMemory",
  "GPUShaderModule",
  "GPUSupportedLimits",
  "GPUTexture",
  "GPUTextureView",
  "NativeVideoFrame",
  "VideoPlayer",
] as const;

type ConstructorName = (typeof CONSTRUCTOR_NAMES)[number];

const NITRO_NAMES: Partial<Record<ConstructorName, string[]>> = {
  GPUError: [
    "GPUError",
    "GPUValidationError",
    "GPUOutOfMemoryError",
    "GPUInternalError",
  ],
  NativeVideoFrame: ["VideoFrame"],
};

const illegalConstructor = (name: string) =>
  function () {
    throw new TypeError(`Illegal constructor: ${name}`);
  };

export const installConstructors = () => {
  const globals = globalThis as unknown as Record<string, unknown>;
  for (const name of CONSTRUCTOR_NAMES) {
    const existing = globals[name];
    const ctor =
      typeof existing === "function" ? existing : illegalConstructor(name);
    const nitroNames = NITRO_NAMES[name] ?? [name];
    Object.defineProperty(ctor, Symbol.hasInstance, {
      configurable: true,
      value: (value: unknown) =>
        (typeof value === "object" &&
          value !== null &&
          NitroModules.isHybridObject(value) &&
          nitroNames.includes(value.name)) ||
        Function.prototype[Symbol.hasInstance].call(ctor, value),
    });
    globals[name] = ctor;
  }
};
