#include "GPUExternalTexture.h"

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "GPUExternalTextureDescriptor.h"

#include "dawn/common/ExternalTextureParams.h"

#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <poll.h>
#endif

namespace rnwgpu {

#if defined(__ANDROID__)
// Whether Dawn imports an AHardwareBuffer of this format as an
// OpaqueYCbCrAndroid texture. Mirrors FormatFromAHardwareBufferFormat
// (dawn/native/AHBFunctions.cpp): the color and depth formats with a WebGPU
// equivalent import as that format; the YUV formats and the formats outside
// the public enum, which drivers use for the camera and the video decoder,
// import as OpaqueYCbCrAndroid.
static bool isOpaqueYCbCrFormat(uint32_t format) {
  switch (format) {
  case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
  case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
  case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
  case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
  case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
  case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
  case AHARDWAREBUFFER_FORMAT_BLOB:
  case AHARDWAREBUFFER_FORMAT_D16_UNORM:
  case AHARDWAREBUFFER_FORMAT_D24_UNORM:
  case AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT:
  case AHARDWAREBUFFER_FORMAT_D32_FLOAT:
  case AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT:
  case AHARDWAREBUFFER_FORMAT_S8_UINT:
  // Single-channel formats of the API 33 and 34 headers, by value so that
  // older NDKs compile.
  case 0x38: // AHARDWAREBUFFER_FORMAT_R8_UNORM
  case 0x39: // AHARDWAREBUFFER_FORMAT_R16_UINT
  case 0x3a: // AHARDWAREBUFFER_FORMAT_R16G16_UINT
  case 0x3b: // AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM
    return false;
  default:
    return true;
  }
}
#endif

// The color space of a YUV frame with every member known. The producer reports
// what it knows (VideoFrameHandle.colorSpace); the matrix and range it leaves
// out come from `fallbackMatrix` / `fallbackRange`, the platform's best guess
// for the buffer, and the rest follows from the matrix: the primaries of the
// standard that defines it, and the SMPTE 170M transfer function that BT.601,
// BT.709 and BT.2020 SDR video is encoded with. An identity matrix means the
// samples are RGB already, which the identity transfer leaves untouched.
static wgpu::ColorSpaceDawn
resolveColorSpace(const VideoFrameColorSpace &reported,
                  wgpu::ColorSpaceYCbCrMatrixDawn fallbackMatrix,
                  wgpu::ColorSpaceYCbCrRangeDawn fallbackRange) {
  wgpu::ColorSpaceDawn colorSpace{};
  colorSpace.yCbCrMatrix = reported.matrix.value_or(fallbackMatrix);
  colorSpace.yCbCrRange = reported.range.value_or(fallbackRange);
  if (reported.primaries.has_value()) {
    colorSpace.primaries = *reported.primaries;
  } else {
    switch (colorSpace.yCbCrMatrix) {
    case wgpu::ColorSpaceYCbCrMatrixDawn::Rec601:
      colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::Rec601;
      break;
    case wgpu::ColorSpaceYCbCrMatrixDawn::Rec2020:
      colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::Rec2020;
      break;
    default:
      // BT.709 shares the sRGB primaries (Dawn's helper only knows them under
      // that name).
      colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::SRGB;
      break;
    }
  }
  if (reported.transfer.has_value()) {
    colorSpace.transfer = *reported.transfer;
  } else {
    colorSpace.transfer =
        colorSpace.yCbCrMatrix == wgpu::ColorSpaceYCbCrMatrixDawn::Identity
            ? wgpu::ColorSpaceTransferDawn::Identity
            : wgpu::ColorSpaceTransferDawn::SMPTE_170M;
  }
  return colorSpace;
}

// The ExternalTextureDescriptor parameters that convert samples in `source` to
// RGB in the color space the descriptor asks for ("srgb" by default,
// "display-p3"): the YCbCr matrix, the range, the source transfer function,
// the gamut conversion and the destination transfer function. Dawn applies
// them while sampling the external texture. The helper is Dawn's own, the one
// Chromium feeds video frames through.
static dawn::ExternalTextureColorSpaceParams
colorSpaceParams(const wgpu::ColorSpaceDawn &source,
                 const std::optional<std::string> &destination) {
  auto target = destination.has_value() && *destination == "display-p3"
                    ? wgpu::PredefinedColorSpace::DisplayP3
                    : wgpu::PredefinedColorSpace::SRGB;
  dawn::ExternalTextureColorSpaceParams params;
  if (dawn::ComputeExternalTextureParams(source, target, &params) !=
      wgpu::Status::Success) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): the color space of the frame cannot be "
        "converted to '" +
        destination.value_or("srgb") + "'");
  }
  return params;
}

// Dawn dereferences the four arrays unconditionally, so a descriptor always
// carries a full set, the no-op one for frames that are RGB already.
static void
applyColorSpaceParams(wgpu::ExternalTextureDescriptor &descriptor,
                      const dawn::ExternalTextureColorSpaceParams &params) {
  descriptor.yuvToRgbConversionMatrix = params.yuvToRgbConversionMatrix.data();
  descriptor.srcTransferFunctionParameters = params.srcTransferFunction.data();
  descriptor.dstTransferFunctionParameters = params.dstTransferFunction.data();
  descriptor.gamutConversionMatrix = params.gamutConversionMatrix.data();
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
  if (memory.BeginAccess(texture, &begin) != wgpu::Status::Success) {
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

  // 5. Build the ExternalTextureDescriptor. Dawn converts the YUV planes to
  //    RGB in the destination color space while sampling, with the matrix,
  //    range, transfer function and gamut of the frame's color space (from the
  //    CVPixelBuffer attachments; BT.709 limited range, the norm for HD video,
  //    where they are missing). A BGRA frame is RGB already: no-op parameters.
  dawn::ExternalTextureColorSpaceParams params =
      isYuv ? colorSpaceParams(
                  resolveColorSpace(frame.colorSpace,
                                    wgpu::ColorSpaceYCbCrMatrixDawn::Rec709,
                                    wgpu::ColorSpaceYCbCrRangeDawn::Narrow),
                  descriptor->colorSpace)
            : dawn::GetNoopColorSpaceParams();
  wgpu::ExternalTextureDescriptor extDesc{};
  if (!label.empty()) {
    extDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  extDesc.plane0 = plane0;
  if (isYuv) {
    extDesc.plane1 = plane1;
  }
  applyColorSpaceParams(extDesc, params);
  extDesc.cropOrigin = {0, 0};
  extDesc.cropSize = {frame.width, frame.height};
  extDesc.apparentSize = {frame.width, frame.height};
  extDesc.mirrored = descriptor->mirrored.value_or(false);
  extDesc.rotation =
      toExternalTextureRotation(descriptor->rotation.value_or(0));

  auto external = device.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    endAccess(memory, texture);
    throw std::runtime_error(
        "GPUExternalTexture::Create(): CreateExternalTexture returned "
        "null");
  }

  return std::make_shared<GPUExternalTexture>(
      std::move(external), std::move(memory), std::move(texture),
      std::move(descriptor->source), std::move(label));
#elif defined(__ANDROID__)
  // 0. A YUV frame imports as an OpaqueYCbCrAndroid texture, which Dawn only
  //    accepts on a device with OpaqueYCbCrAndroidForExternalTexture.
  //    requestDevice() enables the feature whenever the adapter supports it;
  //    a device created by another library (importDevice) has to request it
  //    itself. Fail with the cause here: Dawn's own report is a bare
  //    "Unsupported texture format TextureFormat::OpaqueYCbCrAndroid" device
  //    error, followed by the failure of every call made with the result.
  if (!device.HasFeature(
          wgpu::FeatureName::OpaqueYCbCrAndroidForExternalTexture)) {
    AHardwareBuffer_Desc ahbDesc{};
    AHardwareBuffer_describe(static_cast<AHardwareBuffer *>(frame.handle),
                             &ahbDesc);
    if (isOpaqueYCbCrFormat(ahbDesc.format)) {
      throw std::runtime_error(
          "GPUExternalTexture::Create(): a YUV frame can only be imported on "
          "a device with the 'opaque-ycbcr-android-for-external-texture' "
          "feature. requestDevice() enables it whenever the adapter supports "
          "it; a device imported from another library has to request it "
          "itself.");
    }
  }

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
  //
  //    The producer of the frame (the video decoder, the camera) may still be
  //    writing it: ImageReader hands a buffer out as soon as it is queued, and
  //    only waits on the producer's fence for CPU access. The fence travels
  //    with the frame as a sync fd; imported as a shared fence, BeginAccess
  //    makes the GPU wait on it before any work that samples the texture.
  //    Dawn duplicates the descriptor on import; the frame keeps its own.
  wgpu::SharedTextureMemoryBeginAccessDescriptor begin{};
  begin.initialized = true;
  begin.concurrentRead = false;
  wgpu::SharedTextureMemoryVkImageLayoutBeginState beginLayout{};
  begin.nextInChain = &beginLayout;
  wgpu::SharedFence acquireFence;
  const uint64_t kBinaryFenceSignaled = 1;
  if (frame.acquireFenceFd >= 0) {
    if (device.HasFeature(wgpu::FeatureName::SharedFenceSyncFD)) {
      wgpu::SharedFenceSyncFDDescriptor fdDesc{};
      fdDesc.handle = frame.acquireFenceFd;
      wgpu::SharedFenceDescriptor fenceDesc{};
      fenceDesc.nextInChain = &fdDesc;
      acquireFence = device.ImportSharedFence(&fenceDesc);
      if (acquireFence) {
        begin.fenceCount = 1;
        begin.fences = &acquireFence;
        begin.signaledValueCount = 1;
        begin.signaledValues = &kBinaryFenceSignaled;
      }
    } else {
      // A device without shared-fence-sync-fd (imported from another library
      // that did not request it) cannot wait on the GPU: wait here instead. A
      // sync fd becomes readable once it signals.
      pollfd pfd{};
      pfd.fd = frame.acquireFenceFd;
      pfd.events = POLLIN;
      int polled = poll(&pfd, 1, 3000);
      if (polled <= 0) {
        __android_log_print(ANDROID_LOG_WARN, "GPUExternalTexture",
                            "the acquire fence of the frame did not signal "
                            "within 3s; sampling it anyway");
      }
    }
  }
  if (memory.BeginAccess(texture, &begin) != wgpu::Status::Success) {
    throw std::runtime_error(
        "GPUExternalTexture::Create(): BeginAccess failed");
  }

  // 4. The color conversion. Dawn imports every YUV AHardwareBuffer as an
  //    OpaqueYCbCrAndroid texture and samples it through a Vulkan YCbCr
  //    sampler whose own conversion is the identity (SamplerVk.cpp), so the
  //    sample is raw Y, Cb, Cr; the external texture then applies the
  //    descriptor's YCbCr matrix, transfer functions and gamut conversion
  //    while sampling (Dawn's YUV AHB specialization), the same way it does
  //    for the biplanar textures of the other backends. The frame's color
  //    space comes from its producer (Image.getDataSpace on API 33, the
  //    container's color info before that); what it leaves unknown falls back
  //    to the YCbCr model and range the driver suggests for the buffer, read
  //    from the shared memory's properties, and to BT.601 limited range, the
  //    camera norm, when the driver has no opinion. A color-format AHB (RGBA8,
  //    R8, ...) samples as RGB directly: no-op parameters.
  dawn::ExternalTextureColorSpaceParams params =
      dawn::GetNoopColorSpaceParams();
  if (texture.GetFormat() == wgpu::TextureFormat::OpaqueYCbCrAndroid) {
    auto fallbackMatrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec601;
    auto fallbackRange = wgpu::ColorSpaceYCbCrRangeDawn::Narrow;
    wgpu::SharedTextureMemoryAHardwareBufferProperties ahbProps{};
    wgpu::SharedTextureMemoryProperties props{};
    props.nextInChain = &ahbProps;
    if (memory.GetProperties(&props) == wgpu::Status::Success) {
      // VkSamplerYcbcrModelConversion / VkSamplerYcbcrRange values, inlined
      // to keep this cross-platform file free of Vulkan headers.
      switch (ahbProps.yCbCrInfo.vkYCbCrModel) {
      case 0: // VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY: RGB data
        fallbackMatrix = wgpu::ColorSpaceYCbCrMatrixDawn::Identity;
        fallbackRange = wgpu::ColorSpaceYCbCrRangeDawn::Identity;
        break;
      case 2: // VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
        fallbackMatrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec709;
        break;
      case 4: // VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020
        fallbackMatrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec2020;
        break;
      default: // YCBCR_601, YCBCR_IDENTITY and unknown models
        break;
      }
      if (fallbackMatrix != wgpu::ColorSpaceYCbCrMatrixDawn::Identity) {
        fallbackRange = ahbProps.yCbCrInfo.vkYCbCrRange == 0
                            ? wgpu::ColorSpaceYCbCrRangeDawn::Full
                            : wgpu::ColorSpaceYCbCrRangeDawn::Narrow;
      }
    }
    params = colorSpaceParams(
        resolveColorSpace(frame.colorSpace, fallbackMatrix, fallbackRange),
        descriptor->colorSpace);
  }

  // 5. Build the ExternalTextureDescriptor: a single (opaque or RGB) plane.
  wgpu::ExternalTextureDescriptor extDesc{};
  if (!label.empty()) {
    extDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  extDesc.cropOrigin = {0, 0};
  extDesc.cropSize = {frame.width, frame.height};
  extDesc.apparentSize = {frame.width, frame.height};
  wgpu::TextureView plane0 = texture.CreateView();
  extDesc.plane0 = plane0;
  applyColorSpaceParams(extDesc, params);
  extDesc.mirrored = descriptor->mirrored.value_or(false);
  extDesc.rotation =
      toExternalTextureRotation(descriptor->rotation.value_or(0));

  auto external = device.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    endAccess(memory, texture);
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
