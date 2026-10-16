#include "NativeFrameImport.h"

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "GPUExternalTextureDescriptor.h"

#include "dawn/common/ExternalTextureParams.h"

#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <android/sync.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace rnwgpu {
namespace native_frame {

namespace {

std::string prefix(const char *caller) { return std::string(caller) + "(): "; }

#if defined(__ANDROID__)
// VK_IMAGE_LAYOUT_UNDEFINED / VK_IMAGE_LAYOUT_GENERAL, inlined to keep this
// cross-platform file free of Vulkan headers.
constexpr uint32_t kVkImageLayoutUndefined = 0;
constexpr uint32_t kVkImageLayoutGeneral = 1;

// Whether Dawn imports an AHardwareBuffer of this format as an
// OpaqueYCbCrAndroid texture. Mirrors FormatFromAHardwareBufferFormat
// (dawn/native/AHBFunctions.cpp): the color and depth formats with a WebGPU
// equivalent import as that format; the YUV formats and the formats outside
// the public enum, which drivers use for the camera and the video decoder,
// import as OpaqueYCbCrAndroid.
bool isOpaqueYCbCrFormat(uint32_t format) {
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

// A sync fd owned by this scope.
struct UniqueFd {
  int fd = -1;
  UniqueFd() = default;
  explicit UniqueFd(int f) : fd(f) {}
  UniqueFd(const UniqueFd &) = delete;
  UniqueFd &operator=(const UniqueFd &) = delete;
  UniqueFd(UniqueFd &&other) noexcept : fd(other.fd) { other.fd = -1; }
  UniqueFd &operator=(UniqueFd &&other) noexcept {
    if (this != &other) {
      reset();
      fd = other.fd;
      other.fd = -1;
    }
    return *this;
  }
  ~UniqueFd() { reset(); }
  void reset() {
    if (fd >= 0) {
      ::close(fd);
    }
    fd = -1;
  }
};
#endif

// The color space of a YUV frame with every member known. The producer reports
// what it knows (VideoFrameHandle.colorSpace), the caller may override members
// (GPUExternalTextureDescriptor.sourceColorSpace), and the matrix and range
// both leave out come from `fallbackMatrix` / `fallbackRange`, the platform's
// best guess for the buffer. The rest follows from the matrix: the primaries
// of the standard that defines it, and the SMPTE 170M transfer function that
// BT.601, BT.709 and BT.2020 SDR video is encoded with. An identity matrix
// means the samples are RGB already, which the identity transfer leaves
// untouched.
wgpu::ColorSpaceDawn
resolveColorSpace(const VideoFrameColorSpace &reported,
                  const std::optional<VideoFrameColorSpace> &override,
                  wgpu::ColorSpaceYCbCrMatrixDawn fallbackMatrix,
                  wgpu::ColorSpaceYCbCrRangeDawn fallbackRange) {
  VideoFrameColorSpace source = reported;
  if (override.has_value()) {
    if (override->matrix.has_value()) {
      source.matrix = override->matrix;
    }
    if (override->range.has_value()) {
      source.range = override->range;
    }
    if (override->primaries.has_value()) {
      source.primaries = override->primaries;
    }
    if (override->transfer.has_value()) {
      source.transfer = override->transfer;
    }
  }
  wgpu::ColorSpaceDawn colorSpace{};
  colorSpace.yCbCrMatrix = source.matrix.value_or(fallbackMatrix);
  colorSpace.yCbCrRange = source.range.value_or(fallbackRange);
  if (source.primaries.has_value()) {
    colorSpace.primaries = *source.primaries;
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
  if (source.transfer.has_value()) {
    colorSpace.transfer = *source.transfer;
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
dawn::ExternalTextureColorSpaceParams
colorSpaceParams(const wgpu::ColorSpaceDawn &source,
                 const std::optional<std::string> &destination,
                 const char *caller) {
  auto target = destination.has_value() && *destination == "display-p3"
                    ? wgpu::PredefinedColorSpace::DisplayP3
                    : wgpu::PredefinedColorSpace::SRGB;
  dawn::ExternalTextureColorSpaceParams params;
  if (dawn::ComputeExternalTextureParams(source, target, &params) !=
      wgpu::Status::Success) {
    throw std::runtime_error(
        prefix(caller) +
        "the color space of the frame cannot be converted to '" +
        destination.value_or("srgb") + "'");
  }
  return params;
}

// Dawn dereferences the four arrays unconditionally, so a descriptor always
// carries a full set, the no-op one for frames that are RGB already.
void applyColorSpaceParams(
    wgpu::ExternalTextureDescriptor &descriptor,
    const dawn::ExternalTextureColorSpaceParams &params) {
  descriptor.yuvToRgbConversionMatrix = params.yuvToRgbConversionMatrix.data();
  descriptor.srcTransferFunctionParameters = params.srcTransferFunction.data();
  descriptor.dstTransferFunctionParameters = params.dstTransferFunction.data();
  descriptor.gamutConversionMatrix = params.gamutConversionMatrix.data();
}

// Map a rotation in degrees (0 / 90 / 180 / 270) to Dawn's enum. Anything that
// isn't a clean multiple of 90 snaps to the nearest quadrant; Dawn only
// supports those four steps for external textures.
wgpu::ExternalTextureRotation toExternalTextureRotation(double degrees) {
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

// A crop coordinate from JS, truncated like a GPUIntegerCoordinate. Throws
// unless it is a number in [0, max]: a cast of a negative or out of range
// double to uint32_t is undefined.
uint32_t cropCoordinate(double value, uint32_t max, const char *name,
                        const char *caller) {
  double truncated = std::trunc(value);
  if (!std::isfinite(truncated) || truncated < 0 ||
      truncated > static_cast<double>(max)) {
    throw std::runtime_error(prefix(caller) + name + " must be in [0, " +
                             std::to_string(max) + "], got " +
                             std::to_string(value));
  }
  return static_cast<uint32_t>(truncated);
}

} // namespace

wgpu::SharedTextureMemory importMemory(const wgpu::Device &device,
                                       const VideoFrameHandle &frame,
                                       const std::string &label,
                                       const char *caller) {
  if (frame.handle == nullptr) {
    throw std::runtime_error(prefix(caller) + "the frame has been released");
  }
  wgpu::SharedTextureMemoryDescriptor memDesc{};
  if (!label.empty()) {
    memDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
#if defined(__APPLE__)
  // The IOSurface imports as a biplanar texture when it is NV12, as a
  // single-plane one when it is BGRA. ExternalTexture views are sampled-only;
  // storage binding isn't needed and for biplanar formats it would fail
  // validation.
  wgpu::SharedTextureMemoryIOSurfaceDescriptor platformDesc{};
  platformDesc.ioSurface = frame.handle;
  platformDesc.allowStorageBinding = false;
  memDesc.nextInChain = &platformDesc;
#elif defined(__ANDROID__)
  // A YUV buffer imports as an OpaqueYCbCrAndroid texture, which Dawn only
  // accepts on a device with OpaqueYCbCrAndroidForExternalTexture.
  // requestDevice() enables the feature whenever the adapter supports it; a
  // device created by another library (importDevice) has to request it
  // itself. Fail with the cause here: Dawn's own report is a bare
  // "Unsupported texture format TextureFormat::OpaqueYCbCrAndroid" device
  // error, followed by the failure of every call made with the result.
  if (!device.HasFeature(
          wgpu::FeatureName::OpaqueYCbCrAndroidForExternalTexture)) {
    AHardwareBuffer_Desc ahbDesc{};
    AHardwareBuffer_describe(static_cast<AHardwareBuffer *>(frame.handle),
                             &ahbDesc);
    if (isOpaqueYCbCrFormat(ahbDesc.format)) {
      throw std::runtime_error(
          prefix(caller) +
          "a YUV buffer can only be imported on a device with the "
          "'opaque-ycbcr-android-for-external-texture' feature. "
          "requestDevice() enables it whenever the adapter supports it; a "
          "device imported from another library has to request it itself.");
    }
  }
  wgpu::SharedTextureMemoryAHardwareBufferDescriptor platformDesc{};
  platformDesc.handle = frame.handle;
  memDesc.nextInChain = &platformDesc;
#else
  throw std::runtime_error(prefix(caller) + "unsupported platform");
#endif
  auto memory = device.ImportSharedTextureMemory(&memDesc);
  // Dawn reports a failed import (feature missing, unusable handle) as a
  // validation error and returns an error object rather than null, with no
  // format in its properties. Fail the import instead.
  wgpu::SharedTextureMemoryProperties properties{};
  if (memory == nullptr ||
      memory.GetProperties(&properties) != wgpu::Status::Success ||
      properties.format == wgpu::TextureFormat::Undefined) {
    throw std::runtime_error(
        prefix(caller) +
        "the import failed - is the 'shared-texture-memory-iosurface' "
        "(Apple) or 'shared-texture-memory-ahardware-buffer' (Android) "
        "feature enabled on the device, and the handle a usable IOSurface / "
        "AHardwareBuffer?");
  }
  return memory;
}

void beginAccess(const wgpu::SharedTextureMemory &memory,
                 const wgpu::Texture &texture, bool initialized,
                 const std::vector<wgpu::SharedFence> &fences,
                 const std::vector<uint64_t> &signaledValues,
                 const char *caller) {
  wgpu::SharedTextureMemoryBeginAccessDescriptor desc{};
  desc.initialized = initialized;
  desc.concurrentRead = false;
  desc.fenceCount = fences.size();
  desc.fences = fences.empty() ? nullptr : fences.data();
  desc.signaledValueCount = signaledValues.size();
  desc.signaledValues =
      signaledValues.empty() ? nullptr : signaledValues.data();
#if defined(__ANDROID__)
  // Dawn's Vulkan backend validates that the descriptor chains the layout to
  // acquire the image in. For a buffer with pixels to keep, GENERAL on both
  // ends, as Dawn's own Android layer passes: with UNDEFINED, Dawn's acquire
  // barrier starts from UNDEFINED (see ImportedTextureBase::
  // TransitionUsageForPass in TextureVk.cpp), which Vulkan defines as "the
  // contents may be discarded", while the buffer holds the frame its producer
  // just wrote. For a buffer whose contents the caller declares discardable
  // (initialized: false), UNDEFINED is exactly that, and no prior producer
  // put the image in GENERAL.
  wgpu::SharedTextureMemoryVkImageLayoutBeginState vkLayout{};
  vkLayout.oldLayout =
      initialized ? kVkImageLayoutGeneral : kVkImageLayoutUndefined;
  vkLayout.newLayout =
      initialized ? kVkImageLayoutGeneral : kVkImageLayoutUndefined;
  desc.nextInChain = &vkLayout;
#endif
  if (memory.BeginAccess(texture, &desc) != wgpu::Status::Success) {
    throw std::runtime_error(prefix(caller) + "BeginAccess failed");
  }
}

void endAccess(const wgpu::SharedTextureMemory &memory,
               const wgpu::Texture &texture,
               wgpu::SharedTextureMemoryEndAccessState *state,
               const char *caller) {
#if defined(__ANDROID__)
  // Dawn's Vulkan backend writes the released VkImage layouts into a chained
  // SharedTextureMemoryVkImageLayoutEndState; validation requires the chain
  // even when the caller doesn't read the values. The Metal backend only
  // accepts its own optional end state.
  wgpu::SharedTextureMemoryVkImageLayoutEndState vkLayout{};
  state->nextInChain = &vkLayout;
#endif
  auto status = memory.EndAccess(texture, state);
#if defined(__ANDROID__)
  // The chained state is this frame's: unchain it before `state` outlives it.
  state->nextInChain = nullptr;
#endif
  if (status != wgpu::Status::Success) {
    throw std::runtime_error(prefix(caller) + "EndAccess failed");
  }
}

void endAccess(const wgpu::SharedTextureMemory &memory,
               const wgpu::Texture &texture) {
  wgpu::SharedTextureMemoryEndAccessState state{};
#if defined(__ANDROID__)
  wgpu::SharedTextureMemoryVkImageLayoutEndState vkLayout{};
  state.nextInChain = &vkLayout;
#endif
  (void)memory.EndAccess(texture, &state);
#if defined(__ANDROID__)
  state.nextInChain = nullptr;
#endif
}

wgpu::SharedFence importAcquireFence(const wgpu::Device &device,
                                     const VideoFrameHandle &frame) {
#if defined(__ANDROID__)
  // The producer of the frame (the video decoder, the camera) may still be
  // writing it: ImageReader hands a buffer out as soon as it is queued, and
  // only waits on the producer's fence for CPU access. The fence travels with
  // the frame as a sync fd; imported as a shared fence, BeginAccess makes the
  // GPU wait on it before any work that samples the texture. Dawn duplicates
  // the descriptor on import; the frame keeps its own.
  if (frame.acquireFenceFd < 0) {
    return nullptr;
  }
  if (device.HasFeature(wgpu::FeatureName::SharedFenceSyncFD)) {
    wgpu::SharedFenceSyncFDDescriptor fdDesc{};
    fdDesc.handle = frame.acquireFenceFd;
    wgpu::SharedFenceDescriptor fenceDesc{};
    fenceDesc.nextInChain = &fdDesc;
    return device.ImportSharedFence(&fenceDesc);
  }
  // A device without shared-fence-sync-fd (imported from another library that
  // did not request it) cannot wait on the GPU: wait here instead. A sync fd
  // becomes readable once it signals.
  pollfd pfd{};
  pfd.fd = frame.acquireFenceFd;
  pfd.events = POLLIN;
  int polled = poll(&pfd, 1, 3000);
  if (polled <= 0) {
    __android_log_print(ANDROID_LOG_WARN, "GPUExternalTexture",
                        "the acquire fence of the frame did not signal "
                        "within 3s; sampling it anyway");
  }
  return nullptr;
#else
  (void)device;
  (void)frame;
  return nullptr;
#endif
}

wgpu::ExternalTexture createExternalTexture(
    const wgpu::Device &device, const wgpu::SharedTextureMemory &memory,
    const wgpu::Texture &texture, const VideoFrameHandle &frame,
    const GPUExternalTextureDescriptor &descriptor, const std::string &label,
    const char *caller) {
  wgpu::ExternalTextureDescriptor extDesc{};
  if (!label.empty()) {
    extDesc.label = wgpu::StringView(label.c_str(), label.size());
  }
  dawn::ExternalTextureColorSpaceParams params =
      dawn::GetNoopColorSpaceParams();
  // The plane views outlive the synchronous CreateExternalTexture() below.
  wgpu::TextureView plane0;
  wgpu::TextureView plane1;
#if defined(__APPLE__)
  (void)memory;
  // NV12 needs plane0 = R8 luma and plane1 = RG8 chroma; BGRA only plane0.
  // Dawn converts the YUV planes to RGB in the destination color space while
  // sampling, with the matrix, range, transfer function and gamut of the
  // frame's color space (from the CVPixelBuffer attachments; BT.709 limited
  // range, the norm for HD video, where they are missing). A BGRA frame is
  // RGB already: no-op parameters.
  bool isYuv = frame.pixelFormat == VideoPixelFormat::NV12;
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
    params = colorSpaceParams(
        resolveColorSpace(frame.colorSpace, descriptor.sourceColorSpace,
                          wgpu::ColorSpaceYCbCrMatrixDawn::Rec709,
                          wgpu::ColorSpaceYCbCrRangeDawn::Narrow),
        descriptor.colorSpace, caller);
  }
  extDesc.plane0 = plane0;
  if (isYuv) {
    extDesc.plane1 = plane1;
  }
#elif defined(__ANDROID__)
  // Dawn imports every YUV AHardwareBuffer as an OpaqueYCbCrAndroid texture
  // and samples it through a Vulkan YCbCr sampler whose own conversion is the
  // identity (SamplerVk.cpp), so the sample is raw Y, Cb, Cr; the external
  // texture then applies the descriptor's YCbCr matrix, transfer functions and
  // gamut conversion while sampling (Dawn's YUV AHB specialization), the same
  // way it does for the biplanar textures of the other backends. The frame's
  // color space comes from its producer (Image.getDataSpace on API 33, the
  // container's color info before that); what it leaves unknown falls back to
  // the YCbCr model and range the driver suggests for the buffer, read from
  // the shared memory's properties, and to BT.601 limited range, the camera
  // norm, when the driver has no opinion. A color-format AHB (RGBA8, R8, ...)
  // samples as RGB directly: no-op parameters.
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
    params = colorSpaceParams(resolveColorSpace(frame.colorSpace,
                                                descriptor.sourceColorSpace,
                                                fallbackMatrix, fallbackRange),
                              descriptor.colorSpace, caller);
  }
  plane0 = texture.CreateView();
  extDesc.plane0 = plane0;
#else
  (void)memory;
  (void)frame;
  throw std::runtime_error(prefix(caller) + "unsupported platform");
#endif
  applyColorSpaceParams(extDesc, params);

  // The crop, the whole frame by default (the size of the texture: the frame
  // reports it too, but a buffer wrapped from another library may not). It
  // must lie within the frame and not be empty.
  uint32_t width = texture.GetWidth();
  uint32_t height = texture.GetHeight();
  uint32_t cropX = 0;
  uint32_t cropY = 0;
  if (descriptor.cropOrigin.has_value() && *descriptor.cropOrigin) {
    cropX = cropCoordinate((*descriptor.cropOrigin)->x, width, "cropOrigin.x",
                           caller);
    cropY = cropCoordinate((*descriptor.cropOrigin)->y, height, "cropOrigin.y",
                           caller);
  }
  if (cropX >= width || cropY >= height) {
    throw std::runtime_error(
        prefix(caller) + "cropOrigin (" + std::to_string(cropX) + ", " +
        std::to_string(cropY) + ") is outside the " + std::to_string(width) +
        "x" + std::to_string(height) + " frame");
  }
  uint32_t cropWidth = width - cropX;
  uint32_t cropHeight = height - cropY;
  if (descriptor.cropSize.has_value() && *descriptor.cropSize) {
    cropWidth = cropCoordinate((*descriptor.cropSize)->width, cropWidth,
                               "cropSize.width", caller);
    cropHeight = cropCoordinate((*descriptor.cropSize)->height, cropHeight,
                                "cropSize.height", caller);
    if (cropWidth == 0 || cropHeight == 0) {
      throw std::runtime_error(prefix(caller) + "cropSize is empty");
    }
  }
  extDesc.cropOrigin = {cropX, cropY};
  extDesc.cropSize = {cropWidth, cropHeight};
  extDesc.apparentSize = {cropWidth, cropHeight};
  extDesc.mirrored = descriptor.mirrored.value_or(false);
  extDesc.rotation = toExternalTextureRotation(descriptor.rotation.value_or(0));

  auto external = device.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    throw std::runtime_error(prefix(caller) +
                             "CreateExternalTexture returned null");
  }
  return external;
}

bool mergeFences(const wgpu::Device &device,
                 const wgpu::SharedTextureMemoryEndAccessState &state,
                 wgpu::SharedFence *fence, uint64_t *signaledValue,
                 const char *caller) {
  if (state.fenceCount == 0) {
    return false;
  }
  if (state.fenceCount == 1) {
    *fence = state.fences[0];
    *signaledValue = state.signaledValues[0];
    return true;
  }
#if defined(__ANDROID__)
  // Dawn's ExportInfo lends the fd of each fence (the fence closes it): merge
  // duplicates into one sync fd, then import that as the fence to return.
  // Dawn duplicates the fd on import, so the merged one is closed here.
  UniqueFd merged;
  for (size_t i = 0; i < state.fenceCount; i++) {
    wgpu::SharedFence source = state.fences[i];
    wgpu::SharedFenceExportInfo info{};
    wgpu::SharedFenceSyncFDExportInfo fdInfo{};
    info.nextInChain = &fdInfo;
    source.ExportInfo(&info);
    if (info.type != wgpu::SharedFenceType::SyncFD) {
      // Dropping it would hand back a fence that signals before all the GPU
      // work of the access is done, and the producer would reuse the buffer
      // too early.
      throw std::runtime_error(prefix(caller) +
                               "a fence of the access is not a sync fd and "
                               "cannot be merged with the others");
    }
    if (fdInfo.handle < 0) {
      // A sync fd of -1 is a fence that has signaled already.
      continue;
    }
    if (merged.fd < 0) {
      merged = UniqueFd(::fcntl(fdInfo.handle, F_DUPFD_CLOEXEC, 0));
    } else {
      UniqueFd next(sync_merge("rnwgpu-end-access", merged.fd, fdInfo.handle));
      merged = std::move(next);
    }
    if (merged.fd < 0) {
      throw std::runtime_error(prefix(caller) +
                               "merging the fences of the access failed");
    }
  }
  if (merged.fd < 0) {
    return false;
  }
  wgpu::SharedFenceSyncFDDescriptor fdDesc{};
  fdDesc.handle = merged.fd;
  wgpu::SharedFenceDescriptor fenceDesc{};
  fenceDesc.nextInChain = &fdDesc;
  auto imported = device.ImportSharedFence(&fenceDesc);
  if (imported == nullptr) {
    throw std::runtime_error(prefix(caller) +
                             "importing the merged fence of the access failed");
  }
  *fence = std::move(imported);
  *signaledValue = 1;
  return true;
#else
  // Metal: the fences of one shared event are covered by its largest value.
  // Dawn's fences are all the device's event, but a fence the caller passed to
  // beginAccess() comes back unconsumed when the access did not use the
  // texture, and it may be on another event: two events can't be merged into
  // one fence, and dropping either would let the producer reuse the buffer
  // too early.
  (void)device;
  void *event = nullptr;
  size_t last = 0;
  for (size_t i = 0; i < state.fenceCount; i++) {
    wgpu::SharedFence source = state.fences[i];
    wgpu::SharedFenceExportInfo info{};
    wgpu::SharedFenceMTLSharedEventExportInfo eventInfo{};
    info.nextInChain = &eventInfo;
    source.ExportInfo(&info);
    if (info.type != wgpu::SharedFenceType::MTLSharedEvent ||
        (i > 0 && eventInfo.sharedEvent != event)) {
      throw std::runtime_error(prefix(caller) +
                               "the fences of the access are on different "
                               "shared events and cannot be merged into one");
    }
    event = eventInfo.sharedEvent;
    if (state.signaledValues[i] > state.signaledValues[last]) {
      last = i;
    }
  }
  *fence = state.fences[last];
  *signaledValue = state.signaledValues[last];
  return true;
#endif
}

} // namespace native_frame
} // namespace rnwgpu
