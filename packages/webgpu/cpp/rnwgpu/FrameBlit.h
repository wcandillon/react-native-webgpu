#pragma once

#include <algorithm>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// Draws a texture into another of the same format, texel for texel over the
// region both cover, and clears the rest: a copy that needs RenderAttachment
// on the destination where a copy needs CopyDst. SurfaceInfo blits its
// offscreen frame onto a surface with it, since configuring a surface for
// CopyDst replaces its swapchain, which on Android reconnects the window and
// drops the frame it shows. The pipeline is built for the first device it
// draws with and the destination's format; reset() drops it.
class FrameBlit {
public:
  void encode(const wgpu::Device &device, const wgpu::CommandEncoder &encoder,
              const wgpu::Texture &source, const wgpu::Texture &destination) {
    if (!_pipeline || _format != destination.GetFormat()) {
      _format = destination.GetFormat();
      _pipeline = createPipeline(device, _format);
    }
    wgpu::BindGroupEntry entry = {};
    entry.binding = 0;
    entry.textureView = source.CreateView();
    wgpu::BindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = _pipeline.GetBindGroupLayout(0);
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &entry;
    wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

    wgpu::RenderPassColorAttachment attachment = {};
    attachment.view = destination.CreateView();
    attachment.loadOp = wgpu::LoadOp::Clear;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = {0.0, 0.0, 0.0, 0.0};
    wgpu::RenderPassDescriptor passDesc = {};
    passDesc.label = "offscreen frame blit";
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &attachment;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDesc);
    pass.SetPipeline(_pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.SetScissorRect(0, 0,
                        std::min(source.GetWidth(), destination.GetWidth()),
                        std::min(source.GetHeight(), destination.GetHeight()));
    pass.Draw(3);
    pass.End();
  }

  void reset() { _pipeline = nullptr; }

private:
  static constexpr const char *kShader = R"(
@group(0) @binding(0) var frame: texture_2d<f32>;

@vertex fn vs(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(corners[index], 0.0, 1.0);
}

@fragment fn fs(@builtin(position) position: vec4f) -> @location(0) vec4f {
  return textureLoad(frame, vec2u(position.xy), 0);
}
)";

  static wgpu::RenderPipeline createPipeline(const wgpu::Device &device,
                                             wgpu::TextureFormat format) {
    wgpu::ShaderSourceWGSL wgsl = {};
    wgsl.code = kShader;
    wgpu::ShaderModuleDescriptor moduleDesc = {};
    moduleDesc.label = "offscreen frame blit";
    moduleDesc.nextInChain = &wgsl;
    wgpu::ShaderModule module = device.CreateShaderModule(&moduleDesc);

    wgpu::ColorTargetState target = {};
    target.format = format;
    wgpu::FragmentState fragment = {};
    fragment.module = module;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;
    wgpu::RenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = "offscreen frame blit";
    pipelineDesc.vertex.module = module;
    pipelineDesc.vertex.entryPoint = "vs";
    pipelineDesc.fragment = &fragment;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    return device.CreateRenderPipeline(&pipelineDesc);
  }

  wgpu::RenderPipeline _pipeline;
  wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
};

} // namespace rnwgpu
