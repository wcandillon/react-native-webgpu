/* eslint-disable @typescript-eslint/no-explicit-any */
import type { NativeSurface } from "./Canvas";
import WebGPUNativeModule from "./NativeWebGPUModule";

export * from "./Canvas";
export * from "./WebGPUViewNativeComponent";
export { default as WebGPUModule } from "./NativeWebGPUModule";

declare global {
  // eslint-disable-next-line no-var
  var __WebGPUContextRegistry: Record<number, NativeSurface>;
}

const GPU: any = {};
GPU[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPU";
};

const GPUAdapter: any = {};
GPUAdapter[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUAdapter";
};

const GPUAdapterInfo: any = {};
GPUAdapterInfo[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUAdapterInfo";
};

const GPUBindGroup: any = {};
GPUBindGroup[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUBindGroup";
};

const GPUBindGroupLayout: any = {};
GPUBindGroupLayout[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUBindGroupLayout";
};

const GPUBuffer: any = {};
GPUBuffer[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUBuffer";
};

const GPUCanvasContext: any = {};
GPUCanvasContext[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUCanvasContext";
};

const GPUCommandBuffer: any = {};
GPUCommandBuffer[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUCommandBuffer";
};

const GPUCommandEncoder: any = {};
GPUCommandEncoder[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUCommandEncoder";
};

const GPUCompilationInfo: any = {};
GPUCompilationInfo[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUCompilationInfo";
};

const GPUCompilationMessage: any = {};
GPUCompilationMessage[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUCompilationMessage";
};

const GPUComputePassEncoder: any = {};
GPUComputePassEncoder[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUComputePassEncoder";
};

const GPUComputePipeline: any = {};
GPUComputePipeline[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUComputePipeline";
};

const GPUDevice: any = {};
GPUDevice[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUDevice";
};

const GPUDeviceLostInfo: any = {};
GPUDeviceLostInfo[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUDeviceLostInfo";
};

const GPUError: any = {};
GPUError[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUError";
};

const GPUExternalTexture: any = {};
GPUExternalTexture[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUExternalTexture";
};

const GPUPipelineLayout: any = {};
GPUPipelineLayout[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUPipelineLayout";
};

const GPUQuerySet: any = {};
GPUQuerySet[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUQuerySet";
};

const GPUQueue: any = {};
GPUQueue[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUQueue";
};

const GPURenderBundle: any = {};
GPURenderBundle[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPURenderBundle";
};

const GPURenderBundleEncoder: any = {};
GPURenderBundleEncoder[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPURenderBundleEncoder";
};

const GPURenderPassEncoder: any = {};
GPURenderPassEncoder[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPURenderPassEncoder";
};

const GPURenderPipeline: any = {};
GPURenderPipeline[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPURenderPipeline";
};

const GPUSampler: any = {};
GPUSampler[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUSampler";
};

const GPUShaderModule: any = {};
GPUShaderModule[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUShaderModule";
};

const GPUTexture: any = {};
GPUTexture[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUTexture";
};

const GPUTextureView: any = {};
GPUTextureView[Symbol.hasInstance] = function (instance: object) {
  return "__brand" in instance && instance.__brand === "GPUTextureView";
};

global.GPU = GPU;
global.GPUAdapter = GPUAdapter;
global.GPUAdapterInfo = GPUAdapterInfo;
global.GPUBindGroup = GPUBindGroup;
global.GPUBindGroupLayout = GPUBindGroupLayout;
global.GPUBuffer = GPUBuffer;
global.GPUCanvasContext = GPUCanvasContext;
global.GPUCommandBuffer = GPUCommandBuffer;
global.GPUCommandEncoder = GPUCommandEncoder;
global.GPUCompilationInfo = GPUCompilationInfo;
global.GPUCompilationMessage = GPUCompilationMessage;
global.GPUComputePassEncoder = GPUComputePassEncoder;
global.GPUComputePipeline = GPUComputePipeline;
global.GPUDevice = GPUDevice;
global.GPUDeviceLostInfo = GPUDeviceLostInfo;
global.GPUError = GPUError;
global.GPUExternalTexture = GPUExternalTexture;
global.GPUPipelineLayout = GPUPipelineLayout;
global.GPUQuerySet = GPUQuerySet;
global.GPUQueue = GPUQueue;
global.GPURenderBundle = GPURenderBundle;
global.GPURenderBundleEncoder = GPURenderBundleEncoder;
global.GPURenderPassEncoder = GPURenderPassEncoder;
global.GPURenderPipeline = GPURenderPipeline;
global.GPUSampler = GPUSampler;
global.GPUShaderModule = GPUShaderModule;
global.GPUTexture = GPUTexture;
global.GPUTextureView = GPUTextureView;

WebGPUNativeModule.install();
