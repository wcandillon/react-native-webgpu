#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"
#include "ArrayBuffer.h"
#include "SurfaceRegistry.h"

namespace rnwgpu {

namespace m = margelo;

class Canvas : public m::HybridObject {
public:
  explicit Canvas(const int contextId, void *surface, const int width, const int height)
      : HybridObject("Canvas"), _contextId(contextId), _surface(surface), _width(width),
        _height(height), _clientWidth(width), _clientHeight(height) {}

  int getWidth() { return _width; }
  int getHeight() { return _height; }

  void setWidth(const int width) { _width = width; }
  void setHeight(const int height) { _height = height; }

  int getClientWidth() { return _clientWidth; }
  int getClientHeight() { return _clientHeight; }

  void setClientWidth(const int width) { _clientWidth = width; }

  void setClientHeight(const int height) { _clientHeight = height; }

  void *getSurface() { return _surface; }

  std::shared_ptr<ArrayBuffer> getImageData() {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurfaceInfo(_contextId);
    auto config = info->getConfig();
    auto device = config.device;
    auto texture = info->getCurrentTexture();

    // Create command encoder
    auto commandEncoder = device.CreateCommandEncoder();

    // Calculate bytes per row
    uint32_t bytesPerRow = _width * 4;

    // Create buffer
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = bytesPerRow * _height;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    auto buffer = device.CreateBuffer(&bufferDesc);

    // Copy texture to buffer
    wgpu::ImageCopyTexture imageCopyTexture;
    imageCopyTexture.texture = texture;

    wgpu::ImageCopyBuffer imageCopyBuffer;
    imageCopyBuffer.buffer = buffer;
    imageCopyBuffer.layout.bytesPerRow = bytesPerRow;

    wgpu::Extent3D copySize = {static_cast<uint32_t>(_width), static_cast<uint32_t>(_height), 1};

    commandEncoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &copySize);

    // Submit command encoder
    wgpu::CommandBuffer commands = commandEncoder.Finish();
    device.GetQueue().Submit(1, &commands);


    // Use std::promise to create a future we can return
    auto promise = std::make_shared<std::promise<std::shared_ptr<ArrayBuffer>>>();
    auto future = promise->get_future();

    // Map the buffer and read its contents
    buffer.MapAsync(
        wgpu::MapMode::Read,
        0,
        bufferDesc.size,
        wgpu::CallbackMode::WaitAnyOnly,
        [buffer, promise, this](wgpu::MapAsyncStatus status, const char* message) {
            if (status != wgpu::MapAsyncStatus::Success) {
                promise->set_exception(std::make_exception_ptr(std::runtime_error(message)));
                return;
            }

            // Get the mapped range
            const void* mappedData = buffer.GetConstMappedRange();
            size_t mappedSize = buffer.GetSize();

            // Create a copy of the data
            std::vector<uint8_t> data(static_cast<const uint8_t*>(mappedData),
                                      static_cast<const uint8_t*>(mappedData) + mappedSize);

            buffer.Unmap();

            // Create and set the ArrayBuffer
            auto arrayBuffer = std::make_shared<ArrayBuffer>(data.data(), data.size(), 4);

            // Set the promise value
            promise->set_value(arrayBuffer);
        }
    );

    return future.get();   
  }

  void loadHybridMethods() override {
    registerHybridGetter("surface", &Canvas::getSurface, this);
    registerHybridGetter("width", &Canvas::getWidth, this);
    registerHybridGetter("height", &Canvas::getHeight, this);
    registerHybridGetter("clientWidth", &Canvas::getClientWidth, this);
    registerHybridGetter("clientHeight", &Canvas::getClientHeight, this);
    registerHybridSetter("width", &Canvas::setWidth, this);
    registerHybridSetter("height", &Canvas::setHeight, this);
    registerHybridMethod("getImageData", &Canvas::getImageData, this);
  }

private:
  void *_surface;
  int _width;
  int _height;
  int _clientWidth;
  int _clientHeight;
  int _contextId;
};

} // namespace rnwgpu
