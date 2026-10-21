#include "GPUQueue.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Convertors.h"
#include "EnforceRange.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"
#include "rnwgpu/GpuWorkGate.h"

namespace rnwgpu {

struct BufferSource {
  void *data;
  size_t size;            // in bytes
  size_t bytesPerElement; // 1 for ArrayBuffers
};

void GPUQueue::submit(
    std::vector<std::shared_ptr<GPUCommandBuffer>> commandBuffers) {
  std::vector<wgpu::CommandBuffer> bufs(commandBuffers.size());
  for (size_t i = 0; i < commandBuffers.size(); i++) {
    bufs[i] = commandBuffers[i]->get();
  }
  Convertor conv;
  uint32_t bufs_size;
  if (!conv(bufs_size, bufs.size())) {
    return;
  }
  // Held while the app is in the background (see GpuWorkGate).
  GpuWorkGate::getInstance().run(
      [queue = _instance, bufs = std::move(bufs), bufs_size]() {
        queue.Submit(bufs_size, bufs.data());
      });
}

void GPUQueue::writeBuffer(std::shared_ptr<GPUBuffer> buffer,
                           uint64_t bufferOffset,
                           std::shared_ptr<ArrayBuffer> data,
                           std::optional<uint64_t> dataOffsetElements,
                           std::optional<size_t> sizeElements) {
  wgpu::Buffer buf = buffer->get();
  BufferSource src{.data = data->_data,
                   .size = data->_size,
                   .bytesPerElement = data->_bytesPerElement};

  // Note that in the JS semantics of WebGPU, writeBuffer works in number of
  // elements of the typed arrays.
  if (dataOffsetElements >
      static_cast<uint64_t>(src.size / src.bytesPerElement)) {
    throw std::runtime_error("dataOffset is larger than data's size.");
    return;
  }
  uint64_t dataOffset = dataOffsetElements.value_or(0) * src.bytesPerElement;
  src.data = reinterpret_cast<uint8_t *>(src.data) + dataOffset;
  src.size -= dataOffset;

  // Size defaults to dataSize - dataOffset. Instead of computing in elements,
  // we directly use it in bytes, and convert the provided value, if any, in
  // bytes.
  uint64_t size64 = static_cast<uint64_t>(src.size);
  if (sizeElements.has_value()) {
    if (sizeElements.value() >
        std::numeric_limits<uint64_t>::max() / src.bytesPerElement) {
      throw std::runtime_error("size overflows.");
      return;
    }
    size64 = sizeElements.value() * src.bytesPerElement;
  }

  if (size64 > static_cast<uint64_t>(src.size)) {
    throw std::runtime_error("size + dataOffset is larger than data's size.");
    return;
  }

  if (size64 % 4 != 0) {
    throw std::runtime_error("size is not a multiple of 4 bytes.");

    return;
  }

  assert(size64 <= std::numeric_limits<size_t>::max());
  const auto size = static_cast<size_t>(size64);
  const auto *bytes = static_cast<const uint8_t *>(src.data);
  // Dawn commits its staging past 16 MB of writes: held while the app is in
  // the background (see GpuWorkGate). WebGPU copies the bytes at the call,
  // so the held write takes a copy of them, the ArrayBuffer may change.
  GpuWorkGate::getInstance().runOrHold(
      [&]() { _instance.WriteBuffer(buf, bufferOffset, bytes, size); },
      [queue = _instance, buf, bufferOffset, bytes, size]() {
        std::vector<uint8_t> copy(bytes, bytes + size);
        return [queue, buf, bufferOffset, copy = std::move(copy)]() {
          queue.WriteBuffer(buf, bufferOffset, copy.data(), copy.size());
        };
      });
}

async::AsyncTaskHandle GPUQueue::onSubmittedWorkDone(jsi::Runtime &runtime) {
  auto queue = _instance;
  // Post to the CALLING runtime's context so the promise settles on the
  // thread that requested it (see GPUBuffer::mapAsync).
  auto context =
      async::RuntimeContext::getOrCreate(runtime, _async->instance());
  return context->postTask(
      [queue](const async::AsyncTaskHandle::ResolveFunction &resolve,
              const async::AsyncTaskHandle::RejectFunction &reject) {
        // Registered in order with the submits: one held while the app is in
        // the background (see GpuWorkGate) waits for the held submits before
        // it rather than settling on the idle queue.
        GpuWorkGate::getInstance().run([queue, resolve, reject]() {
          queue.OnSubmittedWorkDone(
              wgpu::CallbackMode::AllowProcessEvents,
              [resolve, reject](wgpu::QueueWorkDoneStatus status,
                                wgpu::StringView message) {
                if (status == wgpu::QueueWorkDoneStatus::Success) {
                  resolve(nullptr);
                } else {
                  std::string error =
                      message.length
                          ? std::string(message.data, message.length)
                          : "Queue work did not complete successfully";
                  reject(std::move(error));
                }
              });
        });
      });
}

void GPUQueue::copyVideoFrameToTexture(
    const GPUImageCopyExternalImage &source,
    const GPUImageCopyTextureTagged &destination, const wgpu::Extent3D &size) {
  if (!destination.texture) {
    throw std::runtime_error(
        "copyExternalImageToTexture: destination.texture is required");
  }
  if (destination.aspect.value_or(wgpu::TextureAspect::All) !=
      wgpu::TextureAspect::All) {
    throw std::runtime_error(
        "copyExternalImageToTexture: a native frame can only be copied into "
        "the color aspect");
  }
  if (size.depthOrArrayLayers > 1) {
    throw std::runtime_error(
        "copyExternalImageToTexture: a native frame is a single 2D image");
  }
  // GPUOrigin2D coordinates are [EnforceRange] unsigned long.
  auto coordinate = [](double value, const char * /*name*/) {
    return enforceRange<uint32_t>(value);
  };
  const double rotation = source.rotation.value_or(0);
  if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
    throw std::runtime_error(
        "copyExternalImageToTexture: rotation must be 0, 90, 180 or 270");
  }

  VideoFrameBlit::Request request;
  request.frame = source.videoFrame;
  if (const auto origin = source.origin.value_or(nullptr)) {
    request.originX = coordinate(origin->x, "origin.x");
    request.originY = coordinate(origin->y, "origin.y");
  }
  request.width = size.width;
  request.height = size.height;
  request.flipY = source.flipY.value_or(false);
  request.rotation = rotation;
  request.mirrored = source.mirrored.value_or(false);
  request.texture = destination.texture->get();
  request.mipLevel = coordinate(destination.mipLevel.value_or(0), "mipLevel");
  wgpu::Origin3D dstOrigin{};
  Convertor conv;
  if (!conv(dstOrigin, destination.origin)) {
    throw std::runtime_error(
        "copyExternalImageToTexture: invalid destination origin");
  }
  if (dstOrigin.z != 0) {
    throw std::runtime_error(
        "copyExternalImageToTexture: a native frame can only be copied into "
        "the first layer of the texture");
  }
  request.dstX = dstOrigin.x;
  request.dstY = dstOrigin.y;
  // The blit submits: held while the app is in the background (see
  // GpuWorkGate), with the frame it copies.
  GpuWorkGate::getInstance().run(
      [blit = _videoFrameBlit, device = _device, queue = _instance, request]() {
        blit->copy(device, queue, request);
      });
}

void GPUQueue::copyExternalImageToTexture(
    std::shared_ptr<GPUImageCopyExternalImage> source,
    std::shared_ptr<GPUImageCopyTextureTagged> destination,
    std::shared_ptr<GPUExtent3D> size) {
  wgpu::TexelCopyTextureInfo dst{};
  wgpu::TexelCopyBufferLayout layout{};
  wgpu::Extent3D sz{};
  Convertor conv;
  if (source->videoFrame) {
    if (!conv(sz, size)) {
      throw std::runtime_error("copyExternalImageToTexture: invalid copy size");
    }
    copyVideoFrameToTexture(*source, *destination, sz);
    return;
  }
  if (!source->source) {
    throw std::runtime_error("copyExternalImageToTexture: the source must be "
                             "an ImageBitmap or a NativeVideoFrame");
  }
  uint32_t bytesPerPixel =
      source->source->getSize() /
      (source->source->getWidth() * source->source->getHeight());
  auto dataLayout = std::make_shared<GPUImageDataLayout>(GPUImageDataLayout{
      std::optional<double>{0.0},
      std::optional<double>{
          static_cast<double>(bytesPerPixel * source->source->getWidth())},
      std::optional<double>{static_cast<double>(source->source->getHeight())}});
  if (!conv(dst.aspect, destination->aspect) ||
      !conv(dst.mipLevel, destination->mipLevel) ||
      !conv(dst.origin, destination->origin) ||
      !conv(dst.texture, destination->texture) ||
      !conv(layout, dataLayout) || //
      !conv(sz, size)) {
    throw std::runtime_error("Invalid input for GPUQueue::writeTexture()");
  }

  const auto origin = source->origin.value_or(nullptr);
  // GPUOrigin2D coordinates are [EnforceRange] unsigned long.
  const size_t sourceOriginX = origin ? enforceRange<uint32_t>(origin->x) : 0;
  const size_t sourceOriginY = origin ? enforceRange<uint32_t>(origin->y) : 0;
  if (sourceOriginX > source->source->getWidth() ||
      sz.width > source->source->getWidth() - sourceOriginX ||
      sourceOriginY > source->source->getHeight() ||
      sz.height > source->source->getHeight() - sourceOriginY ||
      sz.depthOrArrayLayers > 1) {
    throw std::runtime_error(
        "The source copy region is outside the ImageBitmap.");
  }

  const bool flipY = source->flipY.value_or(false);
  // premultipliedAlpha defaults to false per the WebGPU spec: an untagged
  // destination expects straight alpha. Convert only when the ImageBitmap's
  // representation differs, using the same rounding as the reference client.
  const bool sourcePremultiplied = source->source->getPremultiplied();
  const bool destinationPremultiplied =
      destination->premultipliedAlpha.value_or(false);
  const bool needsAlphaConversion =
      bytesPerPixel == 4 && sourcePremultiplied != destinationPremultiplied;

  if (sourceOriginX != 0 || sourceOriginY != 0 || flipY ||
      needsAlphaConversion) {
    uint32_t sourceRowSize = bytesPerPixel * source->source->getWidth();
    uint32_t rowSize = bytesPerPixel * sz.width;
    uint32_t totalSize = rowSize * sz.height;

    // Stage only the selected subregion so transformations never touch the
    // ImageBitmap's backing store or pixels outside the requested copy.
    std::vector<uint8_t> staged(totalSize);
    const uint8_t *src =
        static_cast<const uint8_t *>(source->source->getData());
    for (uint32_t row = 0; row < sz.height; ++row) {
      const uint32_t sourceRow =
          sourceOriginY + (flipY ? sz.height - 1 - row : row);
      const uint32_t sourceOffset =
          sourceRow * sourceRowSize + sourceOriginX * bytesPerPixel;
      std::memcpy(staged.data() + row * rowSize, src + sourceOffset, rowSize);
    }
    if (needsAlphaConversion) {
      convertAlpha(staged.data(), totalSize, sourcePremultiplied,
                   destinationPremultiplied);
    }
    layout.bytesPerRow = rowSize;
    layout.rowsPerImage = sz.height;
    // Held while the app is in the background (see GpuWorkGate), with the
    // staged pixels.
    GpuWorkGate::getInstance().runOrHold(
        [&]() {
          _instance.WriteTexture(&dst, staged.data(), totalSize, &layout, &sz);
        },
        [&]() {
          return [queue = _instance, dst, layout, sz,
                  staged = std::move(staged)]() {
            queue.WriteTexture(&dst, staged.data(), staged.size(), &layout,
                               &sz);
          };
        });
  } else {
    // Same, keeping the ImageBitmap alive for its pixels.
    GpuWorkGate::getInstance().runOrHold(
        [&]() {
          _instance.WriteTexture(&dst, source->source->getData(),
                                 source->source->getSize(), &layout, &sz);
        },
        [&]() {
          return
              [queue = _instance, dst, layout, sz, bitmap = source->source]() {
                queue.WriteTexture(&dst, bitmap->getData(), bitmap->getSize(),
                                   &layout, &sz);
              };
        });
  }
}

// Releases a frame the platform handed over that is not going to be drawn.
static void releaseFrame(VideoFrameHandle &frame) {
  if (frame.deleter) {
    frame.deleter();
    frame.deleter = nullptr;
  }
  frame.handle = nullptr;
}

// A rejected JS promise. A resolve factory cannot reject the task's promise
// itself; resolving it with this value makes it adopt the rejection.
static jsi::Value rejectedPromise(jsi::Runtime &runtime,
                                  const std::string &message) {
  auto promiseConstructor =
      runtime.global().getPropertyAsObject(runtime, "Promise");
  auto reject = promiseConstructor.getPropertyAsFunction(runtime, "reject");
  jsi::JSError error(runtime, message);
  return reject.callWithThis(runtime, promiseConstructor, error.value());
}

async::AsyncTaskHandle GPUQueue::drawElementImageToTexture(
    jsi::Runtime &runtime, std::shared_ptr<GPUDrawElementImageSource> source,
    std::shared_ptr<GPUDrawElementImageDestination> destination) {
  auto platform = PlatformContext::current();
  if (!platform) {
    throw std::runtime_error(
        "drawElementImageToTexture: react-native-webgpu is not installed");
  }
  if (!destination || !destination->info || !destination->info->texture) {
    throw std::runtime_error(
        "drawElementImageToTexture: `destination.texture` is required");
  }
  const auto &info = destination->info;

  // Destination texture: only the 8-bit RGBA/BGRA formats the platform
  // rasterizers produce natively are accepted (with a byte swizzle between
  // the two); anything else would need a GPU blit.
  wgpu::TexelCopyTextureInfo dst{};
  Convertor conv;
  if (!conv(dst.aspect, info->aspect) || !conv(dst.mipLevel, info->mipLevel) ||
      !conv(dst.origin, info->origin) || !conv(dst.texture, info->texture)) {
    throw std::runtime_error("drawElementImageToTexture: invalid destination");
  }
  const wgpu::Texture texture = info->texture->get();
  bool destinationIsBgra = false;
  switch (texture.GetFormat()) {
  case wgpu::TextureFormat::RGBA8Unorm:
  case wgpu::TextureFormat::RGBA8UnormSrgb:
    destinationIsBgra = false;
    break;
  case wgpu::TextureFormat::BGRA8Unorm:
  case wgpu::TextureFormat::BGRA8UnormSrgb:
    destinationIsBgra = true;
    break;
  default:
    throw std::runtime_error(
        "drawElementImageToTexture: the destination texture must be "
        "rgba8unorm, rgba8unorm-srgb, bgra8unorm or bgra8unorm-srgb");
  }
  if (!(texture.GetUsage() & wgpu::TextureUsage::CopyDst)) {
    throw std::runtime_error(
        "drawElementImageToTexture: the destination texture must have "
        "GPUTextureUsage.COPY_DST");
  }
  if (texture.GetDimension() != wgpu::TextureDimension::e2D ||
      texture.GetSampleCount() != 1) {
    throw std::runtime_error(
        "drawElementImageToTexture: the destination must be a single-sampled "
        "2D texture");
  }
  if (dst.mipLevel >= texture.GetMipLevelCount()) {
    throw std::runtime_error(
        "drawElementImageToTexture: destination.mipLevel is out of range");
  }
  const uint32_t mipWidth =
      std::max<uint32_t>(1, texture.GetWidth() >> dst.mipLevel);
  const uint32_t mipHeight =
      std::max<uint32_t>(1, texture.GetHeight() >> dst.mipLevel);
  if (dst.origin.z >= texture.GetDepthOrArrayLayers()) {
    throw std::runtime_error(
        "drawElementImageToTexture: destination.origin.z is out of range");
  }

  // Source rectangle (points, view space). Omitted width/height mean "the
  // whole view", which only the platform knows.
  ViewSnapshotRequest request{};
  request.viewTag = source->viewTag;
  request.sourceX = source->sourceX.value_or(0.0);
  request.sourceY = source->sourceY.value_or(0.0);
  request.sourceWidth = source->sourceWidth.value_or(0.0);
  request.sourceHeight = source->sourceHeight.value_or(0.0);
  if (!std::isfinite(request.sourceX) || !std::isfinite(request.sourceY) ||
      !std::isfinite(request.sourceWidth) ||
      !std::isfinite(request.sourceHeight) ||
      (source->sourceWidth.has_value() && request.sourceWidth <= 0) ||
      (source->sourceHeight.has_value() && request.sourceHeight <= 0)) {
    throw std::runtime_error(
        "drawElementImageToTexture: the source rectangle must be finite with "
        "a positive width and height");
  }

  request.afterScreenUpdates = source->afterScreenUpdates.value_or(true);

  // With RENDER_ATTACHMENT as well, a platform that renders the view into a
  // GPU buffer (Android 13 and above through HardwareRenderer, iOS through an
  // IOSurface) hands that buffer over instead of reading the pixels back, and
  // the view is drawn into the destination on the GPU (VideoFrameBlit), which
  // writes the color aspect of the first layer. An omitted aspect means "all"
  // (dst.aspect itself stays at Dawn's Undefined default, which WriteTexture
  // reads the same way).
  request.allowFrame =
      (texture.GetUsage() & wgpu::TextureUsage::RenderAttachment) != 0 &&
      info->aspect.value_or(wgpu::TextureAspect::All) ==
          wgpu::TextureAspect::All &&
      dst.origin.z == 0;

  // Destination size (pixels). Omitted means the source rectangle's natural
  // pixel size (points times the device pixel ratio), which the platform
  // computes; the bounds check then happens once the pixels are back.
  if (destination->size.has_value()) {
    wgpu::Extent3D size{};
    if (!conv(size, destination->size.value())) {
      throw std::runtime_error(
          "drawElementImageToTexture: invalid destination.size");
    }
    if (size.width == 0 || size.height == 0 || size.depthOrArrayLayers != 1) {
      throw std::runtime_error(
          "drawElementImageToTexture: destination.size must have a positive "
          "width and height and a depthOrArrayLayers of 1");
    }
    if (size.width > mipWidth - std::min(dst.origin.x, mipWidth) ||
        size.height > mipHeight - std::min(dst.origin.y, mipHeight)) {
      throw std::runtime_error(
          "drawElementImageToTexture: destination.origin + destination.size "
          "exceeds the texture's extent at destination.mipLevel");
    }
    request.width = size.width;
    request.height = size.height;
  }

  // premultipliedAlpha defaults to false, like copyExternalImageToTexture.
  const bool destinationPremultiplied =
      info->premultipliedAlpha.value_or(false);

  // Settle on the CALLING runtime's context (see GPUBuffer::mapAsync): the
  // snapshot completes on the UI thread and is deposited into that context's
  // mailbox; the texture write and the resolve then run on the runtime's own
  // thread during its next tick.
  auto context =
      async::RuntimeContext::getOrCreate(runtime, _async->instance());
  auto queue = _instance;
  auto device = _device;
  auto blit = _videoFrameBlit;
  // Keeps the destination texture alive until the write is issued.
  auto gpuTexture = info->texture;
  return context->postTask(
      [platform, request, dst, mipWidth, mipHeight, destinationIsBgra,
       destinationPremultiplied, device, queue, gpuTexture,
       blit](const async::AsyncTaskHandle::ResolveFunction &resolve,
             const async::AsyncTaskHandle::RejectFunction &reject) {
        platform->snapshotView(
            request,
            [resolve, reject, dst, mipWidth, mipHeight, destinationIsBgra,
             destinationPremultiplied, queue,
             gpuTexture](ImageData image) mutable {
              // Arbitrary (UI) thread: validate without touching JSI.
              if (image.width == 0 || image.height == 0 ||
                  image.data.size() < image.width * image.height * 4) {
                reject("drawElementImageToTexture: the view produced no "
                       "pixels (is it mounted with a non-zero size?)");
                return;
              }
              if (image.width >
                      mipWidth - std::min<uint32_t>(dst.origin.x, mipWidth) ||
                  image.height >
                      mipHeight - std::min<uint32_t>(dst.origin.y, mipHeight)) {
                reject("drawElementImageToTexture: the view's natural pixel "
                       "size (" +
                       std::to_string(image.width) + "x" +
                       std::to_string(image.height) +
                       ") does not fit in the destination texture at "
                       "destination.origin; pass destination.size to scale "
                       "it, or create a larger texture");
                return;
              }
              resolve([image = std::move(image), dst, destinationIsBgra,
                       destinationPremultiplied, queue,
                       gpuTexture](jsi::Runtime &) mutable -> jsi::Value {
                // Owning runtime's thread. Bring the pixels into the
                // destination's channel order and alpha representation, then
                // issue the write.
                const bool sourceIsBgra =
                    image.format == wgpu::TextureFormat::BGRA8Unorm ||
                    image.format == wgpu::TextureFormat::BGRA8UnormSrgb;
                uint8_t *pixels = image.data.data();
                const size_t byteLength = image.width * image.height * 4;
                if (sourceIsBgra != destinationIsBgra) {
                  for (size_t i = 0; i + 3 < byteLength; i += 4) {
                    std::swap(pixels[i], pixels[i + 2]);
                  }
                }
                convertAlpha(pixels, byteLength, image.premultiplied,
                             destinationPremultiplied);
                wgpu::TexelCopyBufferLayout layout{};
                layout.offset = 0;
                layout.bytesPerRow = static_cast<uint32_t>(image.width * 4);
                layout.rowsPerImage = static_cast<uint32_t>(image.height);
                wgpu::Extent3D extent{static_cast<uint32_t>(image.width),
                                      static_cast<uint32_t>(image.height), 1};
                // Held while the app is in the background (see GpuWorkGate),
                // with the pixels.
                GpuWorkGate::getInstance().runOrHold(
                    [&]() {
                      queue.WriteTexture(&dst, pixels, byteLength, &layout,
                                         &extent);
                    },
                    [&]() {
                      return [queue, dst, layout, extent, byteLength,
                              image = std::move(image)]() {
                        queue.WriteTexture(&dst, image.data.data(), byteLength,
                                           &layout, &extent);
                      };
                    });
                return jsi::Value::undefined();
              });
            },
            [resolve, reject, dst, mipWidth, mipHeight,
             destinationPremultiplied, device, queue, gpuTexture,
             blit](VideoFrameHandle frame) mutable {
              // Arbitrary (UI) thread: validate without touching JSI. A frame
              // that is not drawn is released here.
              if (frame.handle == nullptr || frame.width == 0 ||
                  frame.height == 0) {
                releaseFrame(frame);
                reject("drawElementImageToTexture: the view produced no "
                       "pixels (is it mounted with a non-zero size?)");
                return;
              }
              if (frame.width >
                      mipWidth - std::min<uint32_t>(dst.origin.x, mipWidth) ||
                  frame.height >
                      mipHeight - std::min<uint32_t>(dst.origin.y, mipHeight)) {
                auto message =
                    "drawElementImageToTexture: the view's natural pixel "
                    "size (" +
                    std::to_string(frame.width) + "x" +
                    std::to_string(frame.height) +
                    ") does not fit in the destination texture at "
                    "destination.origin; pass destination.size to scale "
                    "it, or create a larger texture";
                releaseFrame(frame);
                reject(std::move(message));
                return;
              }
              resolve([frame = std::move(frame), dst, destinationPremultiplied,
                       device, queue, gpuTexture,
                       blit](jsi::Runtime &runtime) mutable -> jsi::Value {
                // Owning runtime's thread. Draw the buffer into the
                // destination region on the GPU: importing it as an external
                // texture makes the GPU wait on the renderer's fence first,
                // and the blit converts the renderer's premultiplied pixels to
                // straight alpha unless the destination asks for premultiplied
                // ones. The frame is released once the draw is submitted (Dawn
                // keeps its own reference to the buffer for in-flight work).
                auto videoFrame =
                    std::make_shared<VideoFrame>(std::move(frame));
                VideoFrameBlit::Request blitRequest;
                blitRequest.frame = videoFrame;
                blitRequest.width = videoFrame->handle().width;
                blitRequest.height = videoFrame->handle().height;
                blitRequest.alpha = destinationPremultiplied
                                        ? VideoFrameBlit::Alpha::Premultiplied
                                        : VideoFrameBlit::Alpha::Straight;
                blitRequest.texture = dst.texture;
                blitRequest.mipLevel = dst.mipLevel;
                blitRequest.dstX = dst.origin.x;
                blitRequest.dstY = dst.origin.y;
                try {
                  blit->copy(device, queue, blitRequest);
                } catch (const std::exception &error) {
                  videoFrame->release();
                  return rejectedPromise(runtime, error.what());
                }
                videoFrame->release();
                return jsi::Value::undefined();
              });
            },
            [reject](std::string error) { reject(std::move(error)); });
      });
}

void GPUQueue::writeTexture(std::shared_ptr<GPUImageCopyTexture> destination,
                            std::shared_ptr<ArrayBuffer> data,
                            std::shared_ptr<GPUImageDataLayout> dataLayout,
                            std::shared_ptr<GPUExtent3D> size) {
  wgpu::TexelCopyTextureInfo dst{};
  wgpu::TexelCopyBufferLayout layout{};
  wgpu::Extent3D sz{};
  Convertor conv;
  if (!conv(dst, destination) ||   //
      !conv(layout, dataLayout) || //
      !conv(sz, size)) {
    throw std::runtime_error("Invalid input for GPUQueue::writeTexture()");
  }

  // Held while the app is in the background (see GpuWorkGate), with a copy
  // of the bytes: WebGPU copies them at the call, the ArrayBuffer may change.
  const auto *bytes = static_cast<const uint8_t *>(data->_data);
  const size_t byteLength = data->_size;
  GpuWorkGate::getInstance().runOrHold(
      [&]() { _instance.WriteTexture(&dst, bytes, byteLength, &layout, &sz); },
      [queue = _instance, dst, layout, sz, bytes, byteLength]() {
        std::vector<uint8_t> copy(bytes, bytes + byteLength);
        return [queue, dst, layout, sz, copy = std::move(copy)]() {
          queue.WriteTexture(&dst, copy.data(), copy.size(), &layout, &sz);
        };
      });
}

} // namespace rnwgpu
