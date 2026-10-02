#include "GPUQueue.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Convertors.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"

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
  _instance.Submit(bufs_size, bufs.data());
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
  _instance.WriteBuffer(buf, bufferOffset, src.data,
                        static_cast<size_t>(size64));
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
        queue.OnSubmittedWorkDone(
            wgpu::CallbackMode::AllowProcessEvents,
            [resolve, reject](wgpu::QueueWorkDoneStatus status,
                              wgpu::StringView message) {
              if (status == wgpu::QueueWorkDoneStatus::Success) {
                resolve(nullptr);
              } else {
                std::string error =
                    message.length ? std::string(message.data, message.length)
                                   : "Queue work did not complete successfully";
                reject(std::move(error));
              }
            });
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
  // GPUOrigin2D coordinates are [EnforceRange] unsigned long: negative,
  // NaN, or out-of-range doubles must be rejected here because casting
  // them to an unsigned integer is undefined behavior.
  constexpr double kMaxOrigin =
      static_cast<double>(std::numeric_limits<uint32_t>::max());
  if (origin && (!(origin->x >= 0) || !(origin->y >= 0) ||
                 origin->x > kMaxOrigin || origin->y > kMaxOrigin)) {
    throw std::runtime_error(
        "The source origin must be a non-negative integer coordinate.");
  }
  const size_t sourceOriginX =
      origin ? static_cast<size_t>(origin->x) : 0;
  const size_t sourceOriginY =
      origin ? static_cast<size_t>(origin->y) : 0;
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
    _instance.WriteTexture(&dst, staged.data(), totalSize, &layout, &sz);
  } else {
    _instance.WriteTexture(&dst, source->source->getData(),
                           source->source->getSize(), &layout, &sz);
  }
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
    throw std::runtime_error(
        "drawElementImageToTexture: invalid destination");
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
  // Keeps the destination texture alive until the write is issued.
  auto gpuTexture = info->texture;
  return context->postTask(
      [platform, request, dst, mipWidth, mipHeight, destinationIsBgra,
       destinationPremultiplied, queue,
       gpuTexture](const async::AsyncTaskHandle::ResolveFunction &resolve,
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
              if (image.width > mipWidth - std::min<uint32_t>(dst.origin.x,
                                                              mipWidth) ||
                  image.height > mipHeight - std::min<uint32_t>(dst.origin.y,
                                                                mipHeight)) {
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
                queue.WriteTexture(&dst, pixels, byteLength, &layout, &extent);
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

  _instance.WriteTexture(&dst, data->_data, data->_size, &layout, &sz);
}

} // namespace rnwgpu
