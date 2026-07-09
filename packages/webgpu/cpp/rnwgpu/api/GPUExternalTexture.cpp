#include "GPUExternalTexture.h"

#include <array>
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

// Identity passthrough for GPUExternalTexture.yuvToRgbMatrix: the sampled
// texel is already RGB (Apple's biplanar path where Dawn converts in the
// sampler transform, RGBA surfaces, or an Android AHB whose driver reports
// RGB_IDENTITY). A 3x4 row-major matrix mapping [c.r, c.g, c.b, 1] to itself.
static constexpr std::array<float, 12> kYuvPassthroughMatrix = {
    1.0f, 0.0f, 0.0f, 0.0f, //
    0.0f, 1.0f, 0.0f, 0.0f, //
    0.0f, 0.0f, 1.0f, 0.0f, //
};

// Build a 3x4 row-major matrix mapping the *sampled* gamma-encoded
// [Y, Cb, Cr, 1] (each normalized to [0,1]) to gamma-encoded R'G'B', from the
// Vulkan-suggested YCbCr model + range of an Android external-format buffer.
// Needed because Dawn's opaque-YCbCr sampling path hard-codes an RGB_IDENTITY
// conversion (SamplerVk.cpp::GetYCbCrForTextureView, crbug.com/497675620), so
// the shader receives raw Y/Cb/Cr and must convert itself.
// The VkSamplerYcbcrModelConversion / VkSamplerYcbcrRange values are inlined
// to avoid a Vulkan header dependency in this cross-platform file.
[[maybe_unused]] static std::array<float, 12>
makeYuvToRgbMatrix(uint32_t vkModel, uint32_t vkRange) {
  constexpr uint32_t kModelRgbIdentity = 0; // VK_..._RGB_IDENTITY
  constexpr uint32_t kModel709 = 2;         // VK_..._YCBCR_709
  constexpr uint32_t kModel601 = 3;         // VK_..._YCBCR_601
  constexpr uint32_t kModel2020 = 4;        // VK_..._YCBCR_2020
  constexpr uint32_t kRangeItuNarrow = 1;   // VK_SAMPLER_YCBCR_RANGE_ITU_NARROW

  if (vkModel == kModelRgbIdentity) {
    // The buffer content is RGB; nothing to convert.
    return kYuvPassthroughMatrix;
  }
  float kr;
  float kb;
  switch (vkModel) {
  case kModel709:
    kr = 0.2126f;
    kb = 0.0722f;
    break;
  case kModel2020:
    kr = 0.2627f;
    kb = 0.0593f;
    break;
  case kModel601:
  default:
    // YCBCR_IDENTITY and unknown models: Android camera streams are BT.601 in
    // practice, so that is the sane default.
    kr = 0.299f;
    kb = 0.114f;
    break;
  }
  const float kg = 1.0f - kr - kb;
  const bool narrow = vkRange == kRangeItuNarrow;
  const float yScale = narrow ? 255.0f / 219.0f : 1.0f;
  const float cScale = narrow ? 255.0f / 224.0f : 1.0f;
  const float yOffset = narrow ? 16.0f / 255.0f : 0.0f;
  const float cOffset = 128.0f / 255.0f;
  const float crR = 2.0f * (1.0f - kr) * cScale;
  const float cbG = -2.0f * kb * (1.0f - kb) / kg * cScale;
  const float crG = -2.0f * kr * (1.0f - kr) / kg * cScale;
  const float cbB = 2.0f * (1.0f - kb) * cScale;
  return {
      yScale, 0.0f, crR,  -yScale * yOffset - crR * cOffset,         //
      yScale, cbG,  crG,  -yScale * yOffset - (cbG + crG) * cOffset, //
      yScale, cbB,  0.0f, -yScale * yOffset - cbB * cOffset,         //
  };
}

// Map a rotation in degrees (0 / 90 / 180 / 270) to Dawn's enum. Anything that
// isn't a clean multiple of 90 snaps to the nearest quadrant; Dawn only
// supports those four steps for external textures.
static wgpu::ExternalTextureRotation toExternalTextureRotation(double degrees) {
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
      std::move(descriptor->source), std::move(label),
      // Dawn's Metal path applies the YUV->RGB conversion (from
      // frame.yuvToRgbMatrix above) inside the sampling transform, so the
      // sampled texel is already RGB.
      kYuvPassthroughMatrix);
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

  // 4. Resolve the YUV->RGB conversion the *shader* must apply. Dawn imports
  //    every YUV / implementation-defined AHB format as OpaqueYCbCrAndroid
  //    (AHBFunctions.cpp::FormatFromAHardwareBufferFormat) and samples it
  //    through a Vulkan SamplerYcbcrConversion that is hard-coded to
  //    RGB_IDENTITY (SamplerVk.cpp::GetYCbCrForTextureView,
  //    crbug.com/497675620), so the sample always comes back as raw
  //    [Y, Cb, Cr]. The driver's *suggested* model + range are captured at
  //    import time though; read them back from the shared memory's properties
  //    and derive the correct conversion matrix, exposed to JS as
  //    GPUExternalTexture.yuvToRgbMatrix. Defined color formats (e.g. an RGBA
  //    AHB -> RGBA8Unorm) sample as RGB directly and get the passthrough.
  std::array<float, 12> yuvToRgbMatrix = kYuvPassthroughMatrix;
  if (texture.GetFormat() == wgpu::TextureFormat::OpaqueYCbCrAndroid) {
    wgpu::SharedTextureMemoryAHardwareBufferProperties ahbProps{};
    wgpu::SharedTextureMemoryProperties props{};
    props.nextInChain = &ahbProps;
    if (memory.GetProperties(&props)) {
      yuvToRgbMatrix = makeYuvToRgbMatrix(ahbProps.yCbCrInfo.vkYCbCrModel,
                                          ahbProps.yCbCrInfo.vkYCbCrRange);
    } else {
      // Fall back to the Android camera norm rather than passthrough.
      yuvToRgbMatrix = makeYuvToRgbMatrix(/* YCBCR_601 */ 3,
                                          /* ITU_NARROW */ 1);
    }
  }

  // 5. Build the ExternalTextureDescriptor: a single (opaque or RGB) plane
  //    with identity transfer. The YUV->RGB conversion cannot run inside
  //    Dawn's external-texture transform here: the single-plane branch of the
  //    Tint transform ignores yuvToRgbConversionMatrix, which is why the
  //    matrix is surfaced to the shader instead. The gamut/transfer arrays
  //    must still be non-null: ComputeExternalTextureParams dereferences them
  //    unconditionally.
  wgpu::ExternalTextureDescriptor extDesc{};
  if (!label.empty()) {
    extDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  extDesc.cropOrigin = {0, 0};
  extDesc.cropSize = {frame.width, frame.height};
  extDesc.apparentSize = {frame.width, frame.height};
  extDesc.gamutConversionMatrix = kIdentityGamutMatrix;
  wgpu::TextureView plane0 = texture.CreateView();
  extDesc.plane0 = plane0;
  extDesc.srcTransferFunctionParameters = kIdentityTransferParams;
  extDesc.dstTransferFunctionParameters = kIdentityTransferParams;
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
      std::move(descriptor->source), std::move(label),
      std::move(yuvToRgbMatrix));
#else
  throw std::runtime_error(
      "GPUExternalTexture::Create(): not yet implemented on this "
      "platform");
#endif
}

} // namespace rnwgpu
