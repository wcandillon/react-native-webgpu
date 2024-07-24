#include "GPUSupportedLimits.h"

namespace rnwgpu {

double GPUSupportedLimits::getMaxTextureDimension1D() {
  return _instance.limits.maxTextureDimension1D;
}

double GPUSupportedLimits::getMaxTextureDimension2D() {
  return _instance.limits.maxTextureDimension2D;
}

double GPUSupportedLimits::getMaxTextureDimension3D() {
  return _instance.limits.maxTextureDimension3D;
}

double GPUSupportedLimits::getMaxTextureArrayLayers() {
  return _instance.limits.maxTextureArrayLayers;
}

double GPUSupportedLimits::getMaxBindGroups() {
  return _instance.limits.maxBindGroups;
}

double GPUSupportedLimits::getMaxBindGroupsPlusVertexBuffers() {
  return _instance.limits.maxBindGroupsPlusVertexBuffers;
}

double GPUSupportedLimits::getMaxBindingsPerBindGroup() {
  return _instance.limits.maxBindingsPerBindGroup;
}

double GPUSupportedLimits::getMaxDynamicUniformBuffersPerPipelineLayout() {
  return _instance.limits.maxDynamicUniformBuffersPerPipelineLayout;
}

double GPUSupportedLimits::getMaxDynamicStorageBuffersPerPipelineLayout() {
  return _instance.limits.maxDynamicStorageBuffersPerPipelineLayout;
}

double GPUSupportedLimits::getMaxSampledTexturesPerShaderStage() {
  return _instance.limits.maxSampledTexturesPerShaderStage;
}

double GPUSupportedLimits::getMaxSamplersPerShaderStage() {
  return _instance.limits.maxSamplersPerShaderStage;
}

double GPUSupportedLimits::getMaxStorageBuffersPerShaderStage() {
  return _instance.limits.maxStorageBuffersPerShaderStage;
}

double GPUSupportedLimits::getMaxStorageTexturesPerShaderStage() {
  return _instance.limits.maxStorageTexturesPerShaderStage;
}

double GPUSupportedLimits::getMaxUniformBuffersPerShaderStage() {
  return _instance.limits.maxUniformBuffersPerShaderStage;
}

double GPUSupportedLimits::getMaxUniformBufferBindingSize() {
  return _instance.limits.maxUniformBufferBindingSize;
}

double GPUSupportedLimits::getMaxStorageBufferBindingSize() {
  return _instance.limits.maxStorageBufferBindingSize;
}

double GPUSupportedLimits::getMinUniformBufferOffsetAlignment() {
  return _instance.limits.minUniformBufferOffsetAlignment;
}

double GPUSupportedLimits::getMinStorageBufferOffsetAlignment() {
  return _instance.limits.minStorageBufferOffsetAlignment;
}

double GPUSupportedLimits::getMaxVertexBuffers() {
  return _instance.limits.maxVertexBuffers;
}

double GPUSupportedLimits::getMaxBufferSize() {
  return _instance.limits.maxBufferSize;
}

double GPUSupportedLimits::getMaxVertexAttributes() {
  return _instance.limits.maxVertexAttributes;
}

double GPUSupportedLimits::getMaxVertexBufferArrayStride() {
  return _instance.limits.maxVertexBufferArrayStride;
}

double GPUSupportedLimits::getMaxInterStageShaderComponents() {
  return _instance.limits.maxInterStageShaderComponents;
}

double GPUSupportedLimits::getMaxInterStageShaderVariables() {
  return _instance.limits.maxInterStageShaderVariables;
}

double GPUSupportedLimits::getMaxColorAttachments() {
  return _instance.limits.maxColorAttachments;
}

double GPUSupportedLimits::getMaxColorAttachmentBytesPerSample() {
  return _instance.limits.maxColorAttachmentBytesPerSample;
}

double GPUSupportedLimits::getMaxComputeWorkgroupStorageSize() {
  return _instance.limits.maxComputeWorkgroupStorageSize;
}

double GPUSupportedLimits::getMaxComputeInvocationsPerWorkgroup() {
  return _instance.limits.maxComputeInvocationsPerWorkgroup;
}

double GPUSupportedLimits::getMaxComputeWorkgroupSizeX() {
  return _instance.limits.maxComputeWorkgroupSizeX;
}

double GPUSupportedLimits::getMaxComputeWorkgroupSizeY() {
  return _instance.limits.maxComputeWorkgroupSizeY;
}

double GPUSupportedLimits::getMaxComputeWorkgroupSizeZ() {
  return _instance.limits.maxComputeWorkgroupSizeZ;
}

double GPUSupportedLimits::getMaxComputeWorkgroupsPerDimension() {
  return _instance.limits.maxComputeWorkgroupsPerDimension;
}

} // namespace rnwgpu
