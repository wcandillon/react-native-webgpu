#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupDescriptor.h"
#include "GPUBindGroupLayoutDescriptor.h"
#include "GPUBindGroupLayoutEntry.h"
#include "GPUBlendComponent.h"
#include "GPUBlendState.h"
#include "GPUBufferBinding.h"
#include "GPUBufferBindingLayout.h"
#include "GPUBufferDescriptor.h"
#include "GPUCanvasConfiguration.h"
#include "GPUColorDict.h"
#include "GPUColorTargetState.h"
#include "GPUCommandBufferDescriptor.h"
#include "GPUCommandEncoderDescriptor.h"
#include "GPUComputePassDescriptor.h"
#include "GPUComputePassTimestampWrites.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUDepthStencilState.h"
#include "GPUDeviceDescriptor.h"
#include "GPUExternalTextureBindingLayout.h"
#include "GPUFragmentState.h"
#include "GPUImageCopyBuffer.h"
#include "GPUImageCopyTexture.h"
#include "GPUImageCopyTextureTagged.h"
#include "GPUImageDataLayout.h"
#include "GPUMultisampleState.h"
#include "GPUPipelineErrorInit.h"
#include "GPUPipelineLayoutDescriptor.h"
#include "GPUPrimitiveState.h"
#include "GPUProgrammableStage.h"
#include "GPUQuerySetDescriptor.h"
#include "GPUQueueDescriptor.h"
#include "GPURenderBundleDescriptor.h"
#include "GPURenderBundleEncoderDescriptor.h"
#include "GPURenderPassColorAttachment.h"
#include "GPURenderPassDepthStencilAttachment.h"
#include "GPURenderPassDescriptor.h"
#include "GPURenderPassTimestampWrites.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPURequestAdapterOptions.h"
#include "GPUSamplerBindingLayout.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModuleCompilationHint.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUStencilFaceState.h"
#include "GPUStorageTextureBindingLayout.h"
#include "GPUTextureBindingLayout.h"
#include "GPUTextureDescriptor.h"
#include "GPUTextureViewDescriptor.h"
#include "GPUVertexAttribute.h"
#include "GPUVertexBufferLayout.h"
#include "GPUVertexState.h"

namespace rnwgpu {

class Convertor {
public:
  ~Convertor() {
    for (auto &free : free_) {
      free();
    }
  }

  template <typename OUT, typename IN>
  [[nodiscard]] inline bool operator()(OUT &&out, IN &&in) {
    return Convert(std::forward<OUT>(out), std::forward<IN>(in));
  }

  template <typename OUT, typename IN>
  [[nodiscard]] inline bool operator()(OUT *&out_els, size_t &out_count,
                                       const std::vector<IN> &in) {
    return Convert(out_els, out_count, in);
  }

  template <typename OUT, typename IN>
  [[nodiscard]] inline bool Convert(OUT *&out_els, size_t &out_count,
                                    const std::vector<IN> &in) {
    if (in.size() == 0) {
      out_els = nullptr;
      out_count = 0;
      return true;
    }
    auto *els = Allocate<std::remove_const_t<OUT>>(in.size());
    for (size_t i = 0; i < in.size(); i++) {
      if (!Convert(els[i], in[i])) {
        return false;
      }
    }
    out_els = els;
    return Convert(out_count, in.size());
  }

  template <typename T> [[nodiscard]] bool Convert(T &out, const T &in) {
    out = in;
    return true;
  }

  template <typename T> [[nodiscard]] bool Convert(T &out, const double &in) {
    out = static_cast<T>(in);
    return true;
  }

  template <typename OUT, typename IN>
  [[nodiscard]] bool Convert(OUT &out, const std::optional<IN> &in) {
    if (in.has_value()) {
      return Convert(out, in.value());
    }
    return true;
  }

  [[nodiscard]] bool conv(wgpu::Origin3D &out,
                          std::shared_ptr<GPUOrigin3D> &in) {
    return Convert(out.x, in->x) && Convert(out.y, in->y) &&
           Convert(out.z, in->z);
  }

  [[nodiscard]] bool Convert(wgpu::BindGroupLayoutEntry &out,
                             const GPUBindGroupLayoutEntry &in) {
    return Convert(out.binding, in.binding) &&
           Convert(out.visibility, in.visibility) &&
           Convert(out.buffer, in.buffer) && Convert(out.sampler, in.sampler) &&
           Convert(out.texture, in.texture) &&
           Convert(out.storageTexture, in.storageTexture);
    // TODO: Implement
    //&& Convert(out.externalTexture, in.externalTexture);
  }

  [[nodiscard]] bool Convert(wgpu::BlendComponent &out,
                             const GPUBlendComponent &in) {
    out = {};
    return Convert(out.operation, in.operation) &&
           Convert(out.dstFactor, in.dstFactor) &&
           Convert(out.srcFactor, in.srcFactor);
  }

  [[nodiscard]] bool Convert(wgpu::BlendState &out, const GPUBlendState &in) {
    out = {};
    return Convert(out.alpha, in.alpha) && Convert(out.color, in.color);
  }

  // TODO: implement
  // [[nodiscard]] bool Convert(wgpu::BufferBinding &out,
  //                            const GPUBufferBinding &in) {
  //   return Convert(out.buffer, in.buffer) && Convert(out.offset, in.offset)
  //   &&
  //          Convert(out.size, in.size);
  // }

  [[nodiscard]] bool Convert(wgpu::BufferBindingLayout &out,
                             const GPUBufferBindingLayout &in) {
    return Convert(out.type, in.type) &&
           Convert(out.hasDynamicOffset, in.hasDynamicOffset) &&
           Convert(out.minBindingSize, in.minBindingSize);
  }

  [[nodiscard]] bool Convert(wgpu::BufferDescriptor &out,
                             const GPUBufferDescriptor &in) {
    return Convert(out.size, in.size) && Convert(out.usage, in.usage) &&
           Convert(out.mappedAtCreation, in.mappedAtCreation) &&
           Convert(out.label, in.label);
  }

  // [[nodiscard]] bool Convert(wgpu::CanvasConfiguration &out,
  //                            const GPUCanvasConfiguration &in) {
  //   return Convert(out.device, in.device) && Convert(out.format, in.format)
  //   &&
  //          Convert(out.usage, in.usage) && Convert(out.viewFormats,
  //          in.viewFormats) && Convert(out.colorSpace, in.colorSpace) &&
  //          Convert(out.alphaMode, in.alphaMode);
  // }

  // [[nodiscard]] bool Convert(wgpu::ColorDict &out, const GPUColorDict &in) {
  //   return Convert(out.r, in.r) && Convert(out.g, in.g) && Convert(out.b,
  //   in.b) &&
  //          Convert(out.a, in.a);
  // }

  [[nodiscard]] bool Convert(wgpu::ColorTargetState &out,
                             const GPUColorTargetState &in) {
    return Convert(out.format, in.format) && Convert(out.blend, in.blend) &&
           Convert(out.writeMask, in.writeMask);
  }

  [[nodiscard]] bool Convert(wgpu::ComputePassDescriptor &out,
                             const GPUComputePassDescriptor &in) {
    return Convert(out.timestampWrites, in.timestampWrites) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::ComputePassTimestampWrites &out,
                             const GPUComputePassTimestampWrites &in) {
    return Convert(out.querySet, in.querySet) &&
           Convert(out.beginningOfPassWriteIndex,
                   in.beginningOfPassWriteIndex) &&
           Convert(out.endOfPassWriteIndex, in.endOfPassWriteIndex);
  }

  [[nodiscard]] bool Convert(wgpu::ComputePipelineDescriptor &out,
                             const GPUComputePipelineDescriptor &in) {
    return Convert(out.compute, in.compute) && Convert(out.layout, in.layout) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::DepthStencilState &out,
                             const GPUDepthStencilState &in) {
    return Convert(out.format, in.format) &&
           Convert(out.depthWriteEnabled, in.depthWriteEnabled) &&
           Convert(out.depthCompare, in.depthCompare) &&
           Convert(out.stencilFront, in.stencilFront) &&
           Convert(out.stencilBack, in.stencilBack) &&
           Convert(out.stencilReadMask, in.stencilReadMask) &&
           Convert(out.stencilWriteMask, in.stencilWriteMask) &&
           Convert(out.depthBias, in.depthBias) &&
           Convert(out.depthBiasSlopeScale, in.depthBiasSlopeScale) &&
           Convert(out.depthBiasClamp, in.depthBiasClamp);
  }

  [[nodiscard]] bool Convert(wgpu::DeviceDescriptor &out,
                             const GPUDeviceDescriptor &in) {
    return Convert(out.requiredFeatures, in.requiredFeatures) &&
           Convert(out.requiredLimits, in.requiredLimits) &&
           Convert(out.defaultQueue, in.defaultQueue) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::ExternalTextureBindingLayout &out,
                             const GPUExternalTextureBindingLayout &in) {
    return;
  }

  [[nodiscard]] bool Convert(wgpu::FragmentState &out,
                             const GPUFragmentState &in) {
    return Convert(out.targets, in.targets) && Convert(out.module, in.module) &&
           Convert(out.entryPoint, in.entryPoint) &&
           Convert(out.constants, in.constants);
  }

  [[nodiscard]] bool Convert(wgpu::ImageCopyBuffer &out,
                             const GPUImageCopyBuffer &in) {
    out = {};
    out.buffer = in.buffer->get();
    return Convert(out.layout.offset, in.offset) &&
           Convert(out.layout.bytesPerRow, in.bytesPerRow) &&
           Convert(out.layout.rowsPerImage, in.rowsPerImage);
  }

  [[nodiscard]] bool Convert(wgpu::ImageCopyTexture &out,
                             const GPUImageCopyTexture &in) {
    return Convert(out.texture, in.texture) &&
           Convert(out.mipLevel, in.mipLevel) &&
           Convert(out.origin, in.origin) && Convert(out.aspect, in.aspect);
  }

  [[nodiscard]] bool Convert(wgpu::TextureDataLayout &out,
                             const GPUImageDataLayout &in) {
    out = {};
    return Convert(out.bytesPerRow, in.bytesPerRow) &&
           Convert(out.offset, in.offset) &&
           Convert(out.rowsPerImage, in.rowsPerImage);
  }

  [[nodiscard]] bool Convert(wgpu::MultisampleState &out,
                             const GPUMultisampleState &in) {
    return Convert(out.count, in.count) && Convert(out.mask, in.mask) &&
           Convert(out.alphaToCoverageEnabled, in.alphaToCoverageEnabled);
  }

  [[nodiscard]] bool Convert(wgpu::PipelineLayoutDescriptor &out,
                             const GPUPipelineLayoutDescriptor &in) {
    return Convert(out.bindGroupLayouts, in.bindGroupLayouts) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::PrimitiveState &out,
                             const GPUPrimitiveState &in) {
    out = {};

    if (in.unclippedDepth) {
      wgpu::PrimitiveDepthClipControl *depthClip =
          Allocate<wgpu::PrimitiveDepthClipControl>();
      depthClip->unclippedDepth = true;
      out.nextInChain = depthClip;
    }

    return Convert(out.topology, in.topology) &&
           Convert(out.stripIndexFormat, in.stripIndexFormat) &&
           Convert(out.frontFace, in.frontFace) &&
           Convert(out.cullMode, in.cullMode);
  }

  [[nodiscard]] bool Convert(wgpu::ProgrammableStageDescriptor &out,
                             const GPUProgrammableStage &in) {
    out = {};
    out.module = in.module->get();

    // Replace nulls in the entryPoint name with another character that's
    // disallowed in WGSL identifiers. This is so that using "main\0" doesn't
    // match an entryPoint named "main".
    out.entryPoint = in.entryPoint
                         ? ConvertStringReplacingNull(in.entryPoint.value())
                         : nullptr;
    // TODO: implement constants
    return false;
    // return Convert(out.constants, out.constantCount, in.constants);
  }

  [[nodiscard]] bool Convert(wgpu::QuerySetDescriptor &out,
                             const GPUQuerySetDescriptor &in) {
    return Convert(out.type, in.type) && Convert(out.count, in.count) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::RenderBundleEncoderDescriptor &out,
                             const GPURenderBundleEncoderDescriptor &in) {
    return Convert(out.depthReadOnly, in.depthReadOnly) &&
           Convert(out.stencilReadOnly, in.stencilReadOnly) &&
           Convert(out.colorFormats, in.colorFormats) &&
           Convert(out.depthStencilFormat, in.depthStencilFormat) &&
           Convert(out.sampleCount, in.sampleCount) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::RenderPassColorAttachment &out,
                             const GPURenderPassColorAttachment &in) {
    return Convert(out.view, in.view) &&
           Convert(out.depthSlice, in.depthSlice) &&
           Convert(out.resolveTarget, in.resolveTarget) &&
           Convert(out.clearValue, in.clearValue) &&
           Convert(out.loadOp, in.loadOp) && Convert(out.storeOp, in.storeOp);
  }

  [[nodiscard]] bool Convert(wgpu::RenderPassDepthStencilAttachment &out,
                             const GPURenderPassDepthStencilAttachment &in) {
    return Convert(out.view, in.view) &&
           Convert(out.depthClearValue, in.depthClearValue) &&
           Convert(out.depthLoadOp, in.depthLoadOp) &&
           Convert(out.depthStoreOp, in.depthStoreOp) &&
           Convert(out.depthReadOnly, in.depthReadOnly) &&
           Convert(out.stencilClearValue, in.stencilClearValue) &&
           Convert(out.stencilLoadOp, in.stencilLoadOp) &&
           Convert(out.stencilStoreOp, in.stencilStoreOp) &&
           Convert(out.stencilReadOnly, in.stencilReadOnly);
  }

  [[nodiscard]] bool Convert(wgpu::RenderPassTimestampWrites &out,
                             const GPURenderPassTimestampWrites &in) {
    return Convert(out.querySet, in.querySet) &&
           Convert(out.beginningOfPassWriteIndex,
                   in.beginningOfPassWriteIndex) &&
           Convert(out.endOfPassWriteIndex, in.endOfPassWriteIndex);
  }

  [[nodiscard]] bool Convert(wgpu::RenderPipelineDescriptor &out,
                             const GPURenderPipelineDescriptor &in) {
    return Convert(out.vertex, in.vertex) &&
           Convert(out.primitive, in.primitive) &&
           Convert(out.depthStencil, in.depthStencil) &&
           Convert(out.multisample, in.multisample) &&
           Convert(out.fragment, in.fragment) &&
           Convert(out.layout, in.layout) && Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::RequestAdapterOptions &out,
                             const GPURequestAdapterOptions &in) {
    return Convert(out.powerPreference, in.powerPreference) &&
           Convert(out.forceFallbackAdapter, in.forceFallbackAdapter);
  }

  [[nodiscard]] bool Convert(wgpu::SamplerBindingLayout &out,
                             const GPUSamplerBindingLayout &in) {
    return Convert(out.type, in.type);
  }

  [[nodiscard]] bool Convert(wgpu::SamplerDescriptor &out,
                             const GPUSamplerDescriptor &in) {
    return Convert(out.addressModeU, in.addressModeU) &&
           Convert(out.addressModeV, in.addressModeV) &&
           Convert(out.addressModeW, in.addressModeW) &&
           Convert(out.magFilter, in.magFilter) &&
           Convert(out.minFilter, in.minFilter) &&
           Convert(out.mipmapFilter, in.mipmapFilter) &&
           Convert(out.lodMinClamp, in.lodMinClamp) &&
           Convert(out.lodMaxClamp, in.lodMaxClamp) &&
           Convert(out.compare, in.compare) &&
           Convert(out.maxAnisotropy, in.maxAnisotropy) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::StencilFaceState &out,
                             const GPUStencilFaceState &in) {
    return Convert(out.compare, in.compare) && Convert(out.failOp, in.failOp) &&
           Convert(out.depthFailOp, in.depthFailOp) &&
           Convert(out.passOp, in.passOp);
  }

  [[nodiscard]] bool Convert(wgpu::StorageTextureBindingLayout &out,
                             const GPUStorageTextureBindingLayout &in) {
    return Convert(out.access, in.access) && Convert(out.format, in.format) &&
           Convert(out.viewDimension, in.viewDimension);
  }

  [[nodiscard]] bool Convert(wgpu::TextureBindingLayout &out,
                             const GPUTextureBindingLayout &in) {
    return Convert(out.sampleType, in.sampleType) &&
           Convert(out.viewDimension, in.viewDimension) &&
           Convert(out.multisampled, in.multisampled);
  }

  [[nodiscard]] bool Convert(wgpu::TextureDescriptor &out,
                             const GPUTextureDescriptor &in) {
    return Convert(out.size, in.size) &&
           Convert(out.mipLevelCount, in.mipLevelCount) &&
           Convert(out.sampleCount, in.sampleCount) &&
           Convert(out.dimension, in.dimension) &&
           Convert(out.format, in.format) && Convert(out.usage, in.usage) &&
           Convert(out.viewFormats, in.viewFormats) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::TextureViewDescriptor &out,
                             const GPUTextureViewDescriptor &in) {
    return Convert(out.format, in.format) &&
           Convert(out.dimension, in.dimension) &&
           Convert(out.aspect, in.aspect) &&
           Convert(out.baseMipLevel, in.baseMipLevel) &&
           Convert(out.mipLevelCount, in.mipLevelCount) &&
           Convert(out.baseArrayLayer, in.baseArrayLayer) &&
           Convert(out.arrayLayerCount, in.arrayLayerCount) &&
           Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::VertexAttribute &out,
                             const GPUVertexAttribute &in) {
    return Convert(out.format, in.format) && Convert(out.offset, in.offset) &&
           Convert(out.shaderLocation, in.shaderLocation);
  }

  [[nodiscard]] bool Convert(wgpu::VertexBufferLayout &out,
                             const GPUVertexBufferLayout &in) {
    out = {};
    return Convert(out.attributes, out.attributeCount, in.attributes) &&
           Convert(out.arrayStride, in.arrayStride) &&
           Convert(out.stepMode, in.stepMode);
  }

  [[nodiscard]] bool Convert(wgpu::VertexState &out, const GPUVertexState &in) {
    out = {};
    // Replace nulls in the entryPoint name with another character that's
    // disallowed in WGSL identifiers. This is so that using "main\0" doesn't
    // match an entryPoint named "main".
    out.entryPoint = in.entryPoint
                         ? ConvertStringReplacingNull(in.entryPoint.value())
                         : nullptr;

    wgpu::VertexBufferLayout *outBuffers = nullptr;
    if (!Convert(out.module, in.module) ||                   //
        !Convert(outBuffers, out.bufferCount, in.buffers) || //
        !Convert(out.constants, out.constantCount, in.constants)) {
      return false;
    }

    // Patch up the unused vertex buffer layouts to use
    // wgpu::VertexStepMode::VertexBufferNotUsed. The converter for optional
    // value will have put the default value of wgpu::VertexBufferLayout that
    // has wgpu::VertexStepMode::Vertex.
    if (in.buffers.has_value()) {
      auto buffers = in.buffers.value();
      out.buffers = outBuffers;
      for (size_t i = 0; i < buffers.size(); i++) {
        if (std::holds_alternative<nullptr_t>(buffers[i])) {
          outBuffers[i] = wgpu::VertexBufferLayout{
              .stepMode = wgpu::VertexStepMode::VertexBufferNotUsed,
          };
        }
      }
    }
    return true;
  }

  [[nodiscard]] bool Convert(wgpu::CommandBufferDescriptor &out,
                             const GPUCommandBufferDescriptor &in) {
    return Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::CommandEncoderDescriptor &out,
                             const GPUCommandEncoderDescriptor &in) {
    return Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::QueueDescriptor &out,
                             const GPUQueueDescriptor &in) {
    return Convert(out.label, in.label);
  }

  [[nodiscard]] bool Convert(wgpu::RenderBundleDescriptor &out,
                             const GPURenderBundleDescriptor &in) {
    return Convert(out.label, in.label);
  }

private:
  char *ConvertStringReplacingNull(std::string_view in) {
    char *out = Allocate<char>(in.size() + 1);
    out[in.size()] = '\0';

    for (size_t i = 0; i < in.size(); i++) {
      if (in[i] == '\0') {
        out[i] = '#';
      } else {
        out[i] = in[i];
      }
    }

    return out;
  }

  template <typename T> T *Allocate(size_t n = 1) {
    auto *ptr = new T[n]{};
    free_.emplace_back([ptr] { delete[] ptr; });
    return ptr;
  }

  std::vector<std::function<void()>> free_;
};

} // namespace rnwgpu
