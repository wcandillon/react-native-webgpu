#pragma once

#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUExternalTexture.h"
#include "GPUExternalTextureDescriptor.h"
#include "VideoFrame.h"

namespace rnwgpu {

// Renders a native video frame into a region of a regular texture. The frame
// is imported as an external texture, so Dawn does the YUV to RGB conversion
// and applies the rotation and the mirroring; a full-screen triangle then
// samples it into the destination. This is what backs
// GPUQueue.copyExternalImageToTexture for NativeVideoFrame sources.
//
// One instance per device, shared by its GPUQueue wrappers (a new wrapper is
// created on every `device.queue` access): it caches the shader, the layouts
// and one pipeline per destination format.
class VideoFrameBlit {
public:
  struct Request {
    std::shared_ptr<VideoFrame> frame;
    // The region of the upright frame (after rotation and mirroring) to copy.
    uint32_t originX = 0;
    uint32_t originY = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool flipY = false;
    double rotation = 0;
    bool mirrored = false;
    // The destination region.
    wgpu::Texture texture;
    uint32_t mipLevel = 0;
    uint32_t dstX = 0;
    uint32_t dstY = 0;
  };

  void copy(const wgpu::Device &device, const wgpu::Queue &queue,
            const Request &request) {
    if (!request.frame || request.frame->handle().handle == nullptr) {
      throw std::runtime_error(
          "copyExternalImageToTexture: the NativeVideoFrame was released");
    }
    if (!request.texture) {
      throw std::runtime_error(
          "copyExternalImageToTexture: destination.texture is required");
    }
    if (!(request.texture.GetUsage() & wgpu::TextureUsage::RenderAttachment)) {
      throw std::runtime_error(
          "copyExternalImageToTexture: a native frame is rendered into the "
          "destination, which needs GPUTextureUsage.RENDER_ATTACHMENT");
    }
    const wgpu::TextureFormat format = request.texture.GetFormat();
    if (!isSupportedDestination(format)) {
      throw std::runtime_error(
          "copyExternalImageToTexture: a native frame can only be copied "
          "into an rgba8unorm, bgra8unorm (or -srgb), rgba16float or "
          "rgb10a2unorm texture");
    }
    if (request.mipLevel >= request.texture.GetMipLevelCount()) {
      throw std::runtime_error(
          "copyExternalImageToTexture: destination.mipLevel is out of range");
    }
    const uint32_t mipWidth =
        std::max(1u, request.texture.GetWidth() >> request.mipLevel);
    const uint32_t mipHeight =
        std::max(1u, request.texture.GetHeight() >> request.mipLevel);
    if (request.width == 0 || request.height == 0) {
      return;
    }
    if (request.dstX > mipWidth || request.width > mipWidth - request.dstX ||
        request.dstY > mipHeight || request.height > mipHeight - request.dstY) {
      throw std::runtime_error(
          "copyExternalImageToTexture: the destination region is outside "
          "the texture");
    }

    // The frame as the shader sees it: Dawn swaps the apparent size of a
    // frame rotated by 90 or 270 degrees.
    const auto &handle = request.frame->handle();
    const int quadrant = static_cast<int>(request.rotation / 90.0) & 3;
    const bool swapped = quadrant == 1 || quadrant == 3;
    const uint32_t frameWidth = swapped ? handle.height : handle.width;
    const uint32_t frameHeight = swapped ? handle.width : handle.height;
    if (request.originX > frameWidth ||
        request.width > frameWidth - request.originX ||
        request.originY > frameHeight ||
        request.height > frameHeight - request.originY) {
      throw std::runtime_error(
          "copyExternalImageToTexture: the source region is outside the "
          "frame");
    }

    // Importing the frame begins the access to its surface; destroy() ends
    // it once the commands that sample it are submitted.
    auto descriptor = std::make_shared<GPUExternalTextureDescriptor>();
    descriptor->source = request.frame;
    descriptor->label = "copyExternalImageToTexture";
    descriptor->rotation = request.rotation;
    descriptor->mirrored = request.mirrored;
    auto external = GPUExternalTexture::Create(device, descriptor);

    const Resources &resources = resourcesFor(device, format);

    // uv = position01 * uvScale + uvOffset, with position01 running from the
    // top-left corner of the copy region; flipY reads the rows bottom-up.
    const float scaleX =
        static_cast<float>(request.width) / static_cast<float>(frameWidth);
    const float scaleY =
        static_cast<float>(request.height) / static_cast<float>(frameHeight);
    const float offsetX =
        static_cast<float>(request.originX) / static_cast<float>(frameWidth);
    const float offsetY =
        static_cast<float>(request.originY) / static_cast<float>(frameHeight);
    std::array<float, 20> uniforms = {};
    const std::vector<double> matrix = external->getYuvToRgbMatrix();
    for (size_t i = 0; i < 12 && i < matrix.size(); i++) {
      uniforms[i] = static_cast<float>(matrix[i]);
    }
    uniforms[12] = scaleX;
    uniforms[13] = request.flipY ? -scaleY : scaleY;
    uniforms[14] = offsetX;
    uniforms[15] = request.flipY ? offsetY + scaleY : offsetY;
    const uint32_t decodeSrgb = isSrgb(format) ? 1u : 0u;
    std::memcpy(&uniforms[16], &decodeSrgb, sizeof(decodeSrgb));

    wgpu::BufferDescriptor bufferDesc = {};
    bufferDesc.label = "copyExternalImageToTexture uniforms";
    bufferDesc.size = sizeof(uniforms);
    bufferDesc.usage = wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = true;
    wgpu::Buffer uniformBuffer = device.CreateBuffer(&bufferDesc);
    std::memcpy(uniformBuffer.GetMappedRange(), uniforms.data(),
                sizeof(uniforms));
    uniformBuffer.Unmap();

    wgpu::ExternalTextureBindingEntry externalEntry = {};
    externalEntry.externalTexture = external->get();
    std::array<wgpu::BindGroupEntry, 3> entries = {};
    entries[0].binding = 0;
    entries[0].nextInChain = &externalEntry;
    entries[1].binding = 1;
    entries[1].sampler = resources.sampler;
    entries[2].binding = 2;
    entries[2].buffer = uniformBuffer;
    entries[2].size = sizeof(uniforms);
    wgpu::BindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = resources.bindGroupLayout;
    bindGroupDesc.entryCount = entries.size();
    bindGroupDesc.entries = entries.data();
    wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

    wgpu::TextureViewDescriptor viewDesc = {};
    viewDesc.baseMipLevel = request.mipLevel;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.dimension = wgpu::TextureViewDimension::e2D;
    wgpu::TextureView target = request.texture.CreateView(&viewDesc);

    // The rest of the texture is left untouched: the pass loads the existing
    // contents and the viewport and scissor confine the draw to the region.
    wgpu::RenderPassColorAttachment attachment = {};
    attachment.view = target;
    attachment.loadOp = wgpu::LoadOp::Load;
    attachment.storeOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor passDesc = {};
    passDesc.label = "copyExternalImageToTexture";
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &attachment;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDesc);
    pass.SetPipeline(resources.pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.SetViewport(static_cast<float>(request.dstX),
                     static_cast<float>(request.dstY),
                     static_cast<float>(request.width),
                     static_cast<float>(request.height), 0.0f, 1.0f);
    pass.SetScissorRect(request.dstX, request.dstY, request.width,
                        request.height);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
    external->destroy();
  }

private:
  struct Resources {
    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::Sampler sampler;
    wgpu::RenderPipeline pipeline;
  };

  static bool isSrgb(wgpu::TextureFormat format) {
    return format == wgpu::TextureFormat::RGBA8UnormSrgb ||
           format == wgpu::TextureFormat::BGRA8UnormSrgb;
  }

  static bool isSupportedDestination(wgpu::TextureFormat format) {
    switch (format) {
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8UnormSrgb:
    case wgpu::TextureFormat::BGRA8Unorm:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
    case wgpu::TextureFormat::RGBA16Float:
    case wgpu::TextureFormat::RGB10A2Unorm:
      return true;
    default:
      return false;
    }
  }

  static constexpr const char *kShader = R"(
struct Uniforms {
  // The rows of the 3x4 matrix mapping the sampled texel to R'G'B'
  // (GPUExternalTexture.yuvToRgbMatrix; the identity outside Android YUV).
  yuvToRgb0: vec4f,
  yuvToRgb1: vec4f,
  yuvToRgb2: vec4f,
  // uv = position01 * uvScale + uvOffset
  uvScale: vec2f,
  uvOffset: vec2f,
  // x: decode sRGB to linear, for an sRGB destination that re-encodes on write
  flags: vec4u,
};

@group(0) @binding(0) var frame: texture_external;
@group(0) @binding(1) var frameSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@vertex fn vs(@builtin(vertex_index) index: u32) -> VsOut {
  // A triangle covering the viewport; position01 runs from its top-left
  // corner, like image coordinates.
  var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  let corner = corners[index];
  var out: VsOut;
  out.position = vec4f(corner, 0.0, 1.0);
  let position01 = vec2f(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5);
  out.uv = position01 * u.uvScale + u.uvOffset;
  return out;
}

fn srgbToLinear(c: vec3f) -> vec3f {
  let low = c / 12.92;
  let high = pow((c + 0.055) / 1.055, vec3f(2.4));
  return select(high, low, c <= vec3f(0.04045));
}

@fragment fn fs(in: VsOut) -> @location(0) vec4f {
  let sampled = textureSampleBaseClampToEdge(frame, frameSampler, in.uv);
  let texel = vec4f(sampled.rgb, 1.0);
  var rgb = vec3f(dot(u.yuvToRgb0, texel), dot(u.yuvToRgb1, texel), dot(u.yuvToRgb2, texel));
  rgb = clamp(rgb, vec3f(0.0), vec3f(1.0));
  if (u.flags.x == 1u) {
    rgb = srgbToLinear(rgb);
  }
  return vec4f(rgb, 1.0);
}
)";

  const Resources &resourcesFor(const wgpu::Device &device,
                                wgpu::TextureFormat format) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _resources.find(static_cast<WGPUTextureFormat>(format));
    if (found != _resources.end()) {
      return found->second;
    }
    if (!_module) {
      wgpu::ShaderSourceWGSL wgsl = {};
      wgsl.code = kShader;
      wgpu::ShaderModuleDescriptor moduleDesc = {};
      moduleDesc.label = "copyExternalImageToTexture";
      moduleDesc.nextInChain = &wgsl;
      _module = device.CreateShaderModule(&moduleDesc);

      wgpu::ExternalTextureBindingLayout externalLayout = {};
      std::array<wgpu::BindGroupLayoutEntry, 3> entries = {};
      entries[0].binding = 0;
      entries[0].visibility = wgpu::ShaderStage::Fragment;
      entries[0].nextInChain = &externalLayout;
      entries[1].binding = 1;
      entries[1].visibility = wgpu::ShaderStage::Fragment;
      entries[1].sampler.type = wgpu::SamplerBindingType::Filtering;
      entries[2].binding = 2;
      entries[2].visibility =
          wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
      entries[2].buffer.type = wgpu::BufferBindingType::Uniform;
      wgpu::BindGroupLayoutDescriptor layoutDesc = {};
      layoutDesc.label = "copyExternalImageToTexture";
      layoutDesc.entryCount = entries.size();
      layoutDesc.entries = entries.data();
      _bindGroupLayout = device.CreateBindGroupLayout(&layoutDesc);

      wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
      pipelineLayoutDesc.bindGroupLayoutCount = 1;
      pipelineLayoutDesc.bindGroupLayouts = &_bindGroupLayout;
      _pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

      wgpu::SamplerDescriptor samplerDesc = {};
      samplerDesc.magFilter = wgpu::FilterMode::Linear;
      samplerDesc.minFilter = wgpu::FilterMode::Linear;
      _sampler = device.CreateSampler(&samplerDesc);
    }

    wgpu::ColorTargetState target = {};
    target.format = format;
    wgpu::FragmentState fragment = {};
    fragment.module = _module;
    fragment.entryPoint = "fs";
    fragment.targetCount = 1;
    fragment.targets = &target;
    wgpu::RenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = "copyExternalImageToTexture";
    pipelineDesc.layout = _pipelineLayout;
    pipelineDesc.vertex.module = _module;
    pipelineDesc.vertex.entryPoint = "vs";
    pipelineDesc.fragment = &fragment;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

    Resources resources;
    resources.bindGroupLayout = _bindGroupLayout;
    resources.sampler = _sampler;
    resources.pipeline = device.CreateRenderPipeline(&pipelineDesc);
    auto inserted = _resources.emplace(static_cast<WGPUTextureFormat>(format),
                                       std::move(resources));
    return inserted.first->second;
  }

  std::mutex _mutex;
  wgpu::ShaderModule _module;
  wgpu::BindGroupLayout _bindGroupLayout;
  wgpu::PipelineLayout _pipelineLayout;
  wgpu::Sampler _sampler;
  std::unordered_map<WGPUTextureFormat, Resources> _resources;
};

} // namespace rnwgpu
