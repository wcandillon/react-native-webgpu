#include "GPUExternalTexture.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "GPUExternalTextureDescriptor.h"

namespace rnwgpu {

// Identity gamut (BT.709 -> sRGB, same primaries) as a 3x3 column-major matrix.
static const float kIdentityGamutMatrix[9] = {
    1.0f, 0.0f, 0.0f, //
    0.0f, 1.0f, 0.0f, //
    0.0f, 0.0f, 1.0f, //
};

// Piecewise gamma transfer-function parameters Dawn expects:
// for |x| < D: y = sign(x) * (C * |x| + F)
// else        : y = sign(x) * (pow(A * |x| + B, G) + E)
// sRGB decode (encoded -> linear).
static const float kSrgbDecodeParams[7] = {
    2.4f,            // G
    1.0f / 1.055f,   // A
    0.055f / 1.055f, // B
    1.0f / 12.92f,   // C
    0.04045f,        // D
    0.0f,            // E
    0.0f,            // F
};
// sRGB encode (linear -> encoded).
static const float kSrgbEncodeParams[7] = {
    1.0f / 2.4f, // G
    1.055f,      // A
    0.0f,        // B
    12.92f,      // C
    0.0031308f,  // D
    -0.055f,     // E
    0.0f,        // F
};

// Identity transfer (y = x). Used when the sampled surface is already in the
// render target's color space: a single-plane BGRA IOSurface, or the Android
// opaque-YCbCr path where the Vulkan sampler already produced RGB. Dawn
// dereferences the transfer-function arrays unconditionally
// (ComputeExternalTextureParams), so these must be non-null even when no
// conversion is wanted.
static const float kIdentityTransferParams[7] = {
    1.0f, // G
    1.0f, // A
    0.0f, // B
    0.0f, // C
    0.0f, // D
    0.0f, // E
    0.0f, // F
};

// BT.709 limited-range YUV -> R'G'B' as a 3x4 row-major matrix mapping
// [Y, Cb, Cr, 1] to gamma-encoded R'G'B' (NOT linear; the sRGB decode in
// srcTransferFunctionParameters linearizes afterwards). Same values the Apple
// NV12 path computes from the CVPixelBuffer; used for Android buffers that
// arrive as a *defined* biplanar format (where we split the planes and convert
// ourselves) rather than an opaque external-format AHB. Camera streams are
// limited-range BT.709 in the overwhelming majority of cases; full-range /
// BT.601 would need different coefficients (refine from the buffer's suggested
// range if it matters).
[[maybe_unused]] static const float kBT709LimitedToRgb[12] = {
    1.164383f, 0.000000f, 1.792741f, -0.972945f,  //
    1.164383f, -0.213249f, -0.532909f, 0.301517f, //
    1.164383f, 2.112402f, 0.000000f, -1.133402f,  //
};

// True for the multi-planar Y + CbCr formats whose planes we can view as
// Plane0Only (luma) / Plane1Only (chroma) and convert with an explicit matrix.
// Excludes OpaqueYCbCrAndroid (external format, no plane views) and triplanar
// formats (would need a third plane). Only referenced on Android.
[[maybe_unused]] static bool isBiplanarYuvFormat(wgpu::TextureFormat format) {
  switch (format) {
  case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
  case wgpu::TextureFormat::R8BG8Biplanar422Unorm:
  case wgpu::TextureFormat::R8BG8Biplanar444Unorm:
  case wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm:
  case wgpu::TextureFormat::R10X6BG10X6Biplanar422Unorm:
  case wgpu::TextureFormat::R10X6BG10X6Biplanar444Unorm:
    return true;
  default:
    return false;
  }
}

// Map a rotation in degrees (0 / 90 / 180 / 270) to Dawn's enum. Anything that
// isn't a clean multiple of 90 snaps to the nearest quadrant; Dawn only
// supports those four steps for external textures.
static wgpu::ExternalTextureRotation
toExternalTextureRotation(double degrees) {
  int quadrant = static_cast<int>(std::lround(degrees / 90.0)) & 3;
  switch (quadrant) {
  case 1:
    return wgpu::ExternalTextureRotation::Rotate90Degrees;
  case 2:
    return wgpu::ExternalTextureRotation::Rotate180Degrees;
  case 3:
    return wgpu::ExternalTextureRotation::Rotate270Degrees;
  default:
    return wgpu::ExternalTextureRotation::Rotate0Degrees;
  }
}

std::shared_ptr<GPUExternalTexture> GPUExternalTexture::Create(
    wgpu::Device device,
    std::shared_ptr<GPUExternalTextureDescriptor> descriptor) {
  if (!descriptor || !descriptor->source) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): descriptor.source (VideoFrame) "
        "is required");
  }
  const auto &source = descriptor->source;
  const auto &frame = source->handle();
  if (frame.handle == nullptr) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): VideoFrame has been released");
  }

#if defined(__APPLE__)
  // 1. Import the IOSurface as SharedTextureMemory. For NV12 surfaces this
  //    yields a biplanar texture; for BGRA, a single-plane one.
  wgpu::SharedTextureMemoryDescriptor memDesc{};
  std::string label = descriptor->label.value_or("external-texture");
  if (!label.empty()) {
    memDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  wgpu::SharedTextureMemoryIOSurfaceDescriptor platformDesc{};
  platformDesc.ioSurface = frame.handle;
  // ExternalTexture views are sampled-only; storage binding isn't needed and
  // for biplanar formats it would fail validation.
  platformDesc.allowStorageBinding = false;
  memDesc.nextInChain = &platformDesc;
  auto memory = device.ImportSharedTextureMemory(&memDesc);
  if (memory == nullptr) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): ImportSharedTextureMemory "
        "returned null. Is 'shared-texture-memory-iosurface' enabled?");
  }

  // 2. Create the texture from the surface. We pass the right format
  //    explicitly so Dawn picks the multi-planar variant on NV12.
  bool isYuv = frame.pixelFormat == VideoPixelFormat::NV12;
  auto texture = memory.CreateTexture();
  if (texture == nullptr) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): CreateTexture returned null");
  }

  // 3. Begin access on the underlying memory. The matching EndAccess runs when
  //    the GPUExternalTexture is destroyed (explicitly via destroy() or at GC).
  wgpu::SharedTextureMemoryBeginAccessDescriptor begin{};
  begin.initialized = true;
  begin.concurrentRead = false;
  if (!memory.BeginAccess(texture, &begin)) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): BeginAccess failed");
  }

  // 4. Build plane views. For NV12 we need plane0 = R8 luma and plane1 = RG8
  //    chroma; for BGRA we only set plane0.
  wgpu::TextureView plane0;
  wgpu::TextureView plane1;
  {
    wgpu::TextureViewDescriptor v{};
    v.aspect =
        isYuv ? wgpu::TextureAspect::Plane0Only : wgpu::TextureAspect::All;
    plane0 = texture.CreateView(&v);
  }
  if (isYuv) {
    wgpu::TextureViewDescriptor v{};
    v.aspect = wgpu::TextureAspect::Plane1Only;
    plane1 = texture.CreateView(&v);
  }

  // 5. Build the ExternalTextureDescriptor. We hand Dawn explicit YUV→RGB and
  //    sRGB transfer-function parameters so the sampler does the full color
  //    conversion in hardware.
  wgpu::ExternalTextureDescriptor extDesc{};
  if (!label.empty()) {
    extDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  extDesc.plane0 = plane0;
  extDesc.gamutConversionMatrix = kIdentityGamutMatrix;
  if (isYuv) {
    extDesc.plane1 = plane1;
    extDesc.yuvToRgbConversionMatrix = frame.yuvToRgbMatrix;
    extDesc.srcTransferFunctionParameters = kSrgbDecodeParams;
    extDesc.dstTransferFunctionParameters = kSrgbEncodeParams;
  } else {
    // BGRA is already RGB in the target color space; pass it through. Dawn
    // dereferences these arrays unconditionally, so they must be non-null.
    extDesc.srcTransferFunctionParameters = kIdentityTransferParams;
    extDesc.dstTransferFunctionParameters = kIdentityTransferParams;
  }
  extDesc.cropOrigin = {0, 0};
  extDesc.cropSize = {frame.width, frame.height};
  extDesc.apparentSize = {frame.width, frame.height};
  extDesc.mirrored = descriptor->mirrored.value_or(false);
  extDesc.rotation =
      toExternalTextureRotation(descriptor->rotation.value_or(0));

  auto external = device.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    wgpu::SharedTextureMemoryEndAccessState state{};
    (void)memory.EndAccess(texture, &state);
    throw std::runtime_error(
        "GPUExternalTexture::Create(): CreateExternalTexture returned "
        "null");
  }

  return std::make_shared<GPUExternalTexture>(
      std::move(external), std::move(memory), std::move(texture),
      std::move(descriptor->source), std::move(label));
#elif defined(__ANDROID__)
  // 1. Import the AHardwareBuffer as SharedTextureMemory. For YUV AHBs this
  //    yields a Dawn texture in the implementation-defined OpaqueYCbCrAndroid
  //    format; for RGBA AHBs, a regular single-plane texture.
  wgpu::SharedTextureMemoryDescriptor memDesc{};
  std::string label = descriptor->label.value_or("external-texture");
  if (!label.empty()) {
    memDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  wgpu::SharedTextureMemoryAHardwareBufferDescriptor platformDesc{};
  platformDesc.handle = frame.handle;
  memDesc.nextInChain = &platformDesc;
  auto memory = device.ImportSharedTextureMemory(&memDesc);
  if (memory == nullptr) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): ImportSharedTextureMemory "
        "returned null. Is 'shared-texture-memory-ahardware-buffer' enabled?");
  }

  // 2. Create the texture. No descriptor: Dawn picks the right format
  //    (OpaqueYCbCrAndroid for YUV, R8 / RGBA8 / ... for color AHBs).
  auto texture = memory.CreateTexture();
  if (texture == nullptr) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): CreateTexture returned null");
  }

  // 3. Begin access. Vulkan requires us to advertise the incoming VkImage
  //    layout (UNDEFINED is fine for the first acquisition of an AHB whose
  //    contents we expect Dawn to read as-is).
  wgpu::SharedTextureMemoryBeginAccessDescriptor begin{};
  begin.initialized = true;
  begin.concurrentRead = false;
  wgpu::SharedTextureMemoryVkImageLayoutBeginState beginLayout{};
  begin.nextInChain = &beginLayout;
  if (!memory.BeginAccess(texture, &begin)) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): BeginAccess failed");
  }

  // 4. Build the ExternalTextureDescriptor. There are two cases depending on
  //    how Dawn imported the AHB (see SharedTextureMemoryVk.cpp):
  //
  //    a. *External* format (camera buffers whose layout has no Vulkan
  //       equivalent) -> OpaqueYCbCrAndroid, a single opaque plane. Sampling
  //       routes through a Vulkan SamplerYcbcrConversion whose model Dawn
  //       copies verbatim from the AHB's suggestedYcbcrModel. We pass a single
  //       plane + identity transfer and let that conversion (if any) run. NOTE:
  //       when the driver reports RGB_IDENTITY the sample comes back as raw
  //       Y/Cb/Cr; there is no public hook to override the model on this path.
  //
  //    b. *Defined* biplanar format (e.g. R8BG8Biplanar420Unorm, exposed by the
  //       dawn-multi-planar-formats feature) -> we split Plane0Only (luma) /
  //       Plane1Only (chroma) and hand Dawn an explicit BT.709 matrix + sRGB
  //       transfer, exactly like the iOS NV12 path. This makes numPlanes == 2
  //       so the matrix is actually applied (the single-plane branch in Dawn's
  //       Tint transform ignores yuvToRgbConversionMatrix).
  //
  //    Either way we must pass non-null gamut/transfer arrays:
  //    ComputeExternalTextureParams dereferences them unconditionally
  //    (kIdentityTransferParams is defined at file scope).
  const bool isBiplanar = frame.pixelFormat == VideoPixelFormat::NV12 &&
                          isBiplanarYuvFormat(texture.GetFormat());

  wgpu::TextureView plane0;
  wgpu::TextureView plane1;
  wgpu::ExternalTextureDescriptor extDesc{};
  if (!label.empty()) {
    extDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  extDesc.cropOrigin = {0, 0};
  extDesc.cropSize = {frame.width, frame.height};
  extDesc.apparentSize = {frame.width, frame.height};
  extDesc.gamutConversionMatrix = kIdentityGamutMatrix;
  if (isBiplanar) {
    wgpu::TextureViewDescriptor v0{};
    v0.aspect = wgpu::TextureAspect::Plane0Only;
    plane0 = texture.CreateView(&v0);
    wgpu::TextureViewDescriptor v1{};
    v1.aspect = wgpu::TextureAspect::Plane1Only;
    plane1 = texture.CreateView(&v1);
    extDesc.plane0 = plane0;
    extDesc.plane1 = plane1;
    extDesc.yuvToRgbConversionMatrix = kBT709LimitedToRgb;
    extDesc.srcTransferFunctionParameters = kSrgbDecodeParams;
    extDesc.dstTransferFunctionParameters = kSrgbEncodeParams;
  } else {
    plane0 = texture.CreateView();
    extDesc.plane0 = plane0;
    extDesc.srcTransferFunctionParameters = kIdentityTransferParams;
    extDesc.dstTransferFunctionParameters = kIdentityTransferParams;
  }
  extDesc.mirrored = descriptor->mirrored.value_or(false);
  extDesc.rotation =
      toExternalTextureRotation(descriptor->rotation.value_or(0));

  auto external = device.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    wgpu::SharedTextureMemoryEndAccessState state{};
    (void)memory.EndAccess(texture, &state);
    throw std::runtime_error(
        "GPUExternalTexture::Create(): CreateExternalTexture returned "
        "null");
  }

  return std::make_shared<GPUExternalTexture>(
      std::move(external), std::move(memory), std::move(texture),
      std::move(descriptor->source), std::move(label));
#else
  throw std::runtime_error(
      "GPUExternalTexture::Create(): not yet implemented on this "
      "platform");
#endif
}

} // namespace rnwgpu
