#pragma once

#include "webgpu/webgpu_cpp.h"

#include "Canvas.h"

namespace rnwgpu {

class OffscreenSurface {
public:
  explicit OffscreenSurface(std::shared_ptr<Canvas> canvas)
      : _canvas(std::move(canvas)) {
#if defined(__ANDROID__)
    _textureFormat = wgpu::TextureFormat::RGBA8Unorm;
#else
    _textureFormat = wgpu::TextureFormat::BGRA8Unorm;
#endif // defined(__ANDROID__)
  }

  void configure(const wgpu::SurfaceConfiguration &config) {
    // Configure the canvas context with the device and format
    _device = config.device;

    wgpu::TextureDescriptor textureDesc;
    textureDesc.size.width = _canvas->getWidth();
    textureDesc.size.height = _canvas->getHeight();
    textureDesc.format = _textureFormat;
    textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                        wgpu::TextureUsage::CopySrc |
                        wgpu::TextureUsage::TextureBinding;

    _texture = _device.CreateTexture(&textureDesc);
  }

  wgpu::Texture getCurrentTexture() {
    if (!_texture) {
      throw std::runtime_error("Texture is not configured");
    }
    return _texture;
  }

private:
  wgpu::TextureFormat _textureFormat;
  wgpu::Texture _texture = nullptr;
  wgpu::Device _device;
  std::shared_ptr<Canvas> _canvas;
};

} // namespace rnwgpu
