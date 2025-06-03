#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUSupportedLimits : public m::HybridObject {
public:
  explicit GPUSupportedLimits(wgpu::Limits instance)
      : HybridObject("GPUSupportedLimits"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  double getMaxTextureDimension1D();
  double getMaxTextureDimension2D();
  double getMaxTextureDimension3D();
  double getMaxTextureArrayLayers();
  double getMaxBindGroups();
  double getMaxBindGroupsPlusVertexBuffers();
  double getMaxBindingsPerBindGroup();
  double getMaxDynamicUniformBuffersPerPipelineLayout();
  double getMaxDynamicStorageBuffersPerPipelineLayout();
  double getMaxSampledTexturesPerShaderStage();
  double getMaxSamplersPerShaderStage();
  double getMaxStorageBuffersPerShaderStage();
  double getMaxStorageTexturesPerShaderStage();
  double getMaxUniformBuffersPerShaderStage();
  double getMaxUniformBufferBindingSize();
  double getMaxStorageBufferBindingSize();
  double getMinUniformBufferOffsetAlignment();
  double getMinStorageBufferOffsetAlignment();
  double getMaxVertexBuffers();
  double getMaxBufferSize();
  double getMaxVertexAttributes();
  double getMaxVertexBufferArrayStride();
  double getMaxInterStageShaderVariables();
  double getMaxColorAttachments();
  double getMaxColorAttachmentBytesPerSample();
  double getMaxComputeWorkgroupStorageSize();
  double getMaxComputeInvocationsPerWorkgroup();
  double getMaxComputeWorkgroupSizeX();
  double getMaxComputeWorkgroupSizeY();
  double getMaxComputeWorkgroupSizeZ();
  double getMaxComputeWorkgroupsPerDimension();

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSupportedLimits::getBrand, this);

    registerHybridGetter("maxTextureDimension1D",
                         &GPUSupportedLimits::getMaxTextureDimension1D, this);
    registerHybridGetter("maxTextureDimension2D",
                         &GPUSupportedLimits::getMaxTextureDimension2D, this);
    registerHybridGetter("maxTextureDimension3D",
                         &GPUSupportedLimits::getMaxTextureDimension3D, this);
    registerHybridGetter("maxTextureArrayLayers",
                         &GPUSupportedLimits::getMaxTextureArrayLayers, this);
    registerHybridGetter("maxBindGroups", &GPUSupportedLimits::getMaxBindGroups,
                         this);
    registerHybridGetter("maxBindGroupsPlusVertexBuffers",
                         &GPUSupportedLimits::getMaxBindGroupsPlusVertexBuffers,
                         this);
    registerHybridGetter("maxBindingsPerBindGroup",
                         &GPUSupportedLimits::getMaxBindingsPerBindGroup, this);
    registerHybridGetter(
        "maxDynamicUniformBuffersPerPipelineLayout",
        &GPUSupportedLimits::getMaxDynamicUniformBuffersPerPipelineLayout,
        this);
    registerHybridGetter(
        "maxDynamicStorageBuffersPerPipelineLayout",
        &GPUSupportedLimits::getMaxDynamicStorageBuffersPerPipelineLayout,
        this);
    registerHybridGetter(
        "maxSampledTexturesPerShaderStage",
        &GPUSupportedLimits::getMaxSampledTexturesPerShaderStage, this);
    registerHybridGetter("maxSamplersPerShaderStage",
                         &GPUSupportedLimits::getMaxSamplersPerShaderStage,
                         this);
    registerHybridGetter(
        "maxStorageBuffersPerShaderStage",
        &GPUSupportedLimits::getMaxStorageBuffersPerShaderStage, this);
    registerHybridGetter(
        "maxStorageTexturesPerShaderStage",
        &GPUSupportedLimits::getMaxStorageTexturesPerShaderStage, this);
    registerHybridGetter(
        "maxUniformBuffersPerShaderStage",
        &GPUSupportedLimits::getMaxUniformBuffersPerShaderStage, this);
    registerHybridGetter("maxUniformBufferBindingSize",
                         &GPUSupportedLimits::getMaxUniformBufferBindingSize,
                         this);
    registerHybridGetter("maxStorageBufferBindingSize",
                         &GPUSupportedLimits::getMaxStorageBufferBindingSize,
                         this);
    registerHybridGetter(
        "minUniformBufferOffsetAlignment",
        &GPUSupportedLimits::getMinUniformBufferOffsetAlignment, this);
    registerHybridGetter(
        "minStorageBufferOffsetAlignment",
        &GPUSupportedLimits::getMinStorageBufferOffsetAlignment, this);
    registerHybridGetter("maxVertexBuffers",
                         &GPUSupportedLimits::getMaxVertexBuffers, this);
    registerHybridGetter("maxBufferSize", &GPUSupportedLimits::getMaxBufferSize,
                         this);
    registerHybridGetter("maxVertexAttributes",
                         &GPUSupportedLimits::getMaxVertexAttributes, this);
    registerHybridGetter("maxVertexBufferArrayStride",
                         &GPUSupportedLimits::getMaxVertexBufferArrayStride,
                         this);
    registerHybridGetter("maxInterStageShaderVariables",
                         &GPUSupportedLimits::getMaxInterStageShaderVariables,
                         this);
    registerHybridGetter("maxColorAttachments",
                         &GPUSupportedLimits::getMaxColorAttachments, this);
    registerHybridGetter(
        "maxColorAttachmentBytesPerSample",
        &GPUSupportedLimits::getMaxColorAttachmentBytesPerSample, this);
    registerHybridGetter("maxComputeWorkgroupStorageSize",
                         &GPUSupportedLimits::getMaxComputeWorkgroupStorageSize,
                         this);
    registerHybridGetter(
        "maxComputeInvocationsPerWorkgroup",
        &GPUSupportedLimits::getMaxComputeInvocationsPerWorkgroup, this);
    registerHybridGetter("maxComputeWorkgroupSizeX",
                         &GPUSupportedLimits::getMaxComputeWorkgroupSizeX,
                         this);
    registerHybridGetter("maxComputeWorkgroupSizeY",
                         &GPUSupportedLimits::getMaxComputeWorkgroupSizeY,
                         this);
    registerHybridGetter("maxComputeWorkgroupSizeZ",
                         &GPUSupportedLimits::getMaxComputeWorkgroupSizeZ,
                         this);
    registerHybridGetter(
        "maxComputeWorkgroupsPerDimension",
        &GPUSupportedLimits::getMaxComputeWorkgroupsPerDimension, this);
  }

  inline const wgpu::Limits get() { return _instance; }

private:
  wgpu::Limits _instance;
};

} // namespace rnwgpu