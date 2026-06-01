#include "GPUDevice.h"

#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Convertors.h"
#include "JSIConverter.h"
#include "PlatformContext.h"

#include "GPUFeatures.h"
#include "GPUInternalError.h"
#include "GPUOutOfMemoryError.h"
#include "GPUValidationError.h"
#include "RnFeatures.h"

namespace rnwgpu {

void GPUDevice::notifyDeviceLost(wgpu::DeviceLostReason reason,
                                 std::string message) {
  if (_lostSettled) {
    return;
  }

  _lostSettled = true;
  _lostInfo = std::make_shared<GPUDeviceLostInfo>(reason, std::move(message));

  if (_lostResolve.has_value()) {
    auto resolve = std::move(*_lostResolve);
    _lostResolve.reset();
    resolve([info = _lostInfo](jsi::Runtime &runtime) mutable {
      return JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(runtime,
                                                                     info);
    });
  }

  _lostHandle.reset();
}

void GPUDevice::forceLossForTesting() {
  // wgpu::StringView view("forceLossForTesting invoked from JS");
  _instance.ForceLoss(wgpu::DeviceLostReason::Unknown,
                      "forceLossForTesting invoked from JS");
}

std::shared_ptr<GPUBuffer>
GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
  wgpu::BufferDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error(
        "GPUDevice::createBuffer(): Error with GPUBufferDescriptor");
  }
  auto result = _instance.CreateBuffer(&desc);
  return std::make_shared<GPUBuffer>(result, _async,
                                     descriptor->label.value_or(""));
}

std::shared_ptr<GPUSupportedLimits> GPUDevice::getLimits() {
  wgpu::Limits limits{};
  if (!_instance.GetLimits(&limits)) {
    throw std::runtime_error("failed to get device limits");
  }
  return std::make_shared<GPUSupportedLimits>(limits);
}

std::shared_ptr<GPUQueue> GPUDevice::getQueue() {
  auto result = _instance.GetQueue();
  return std::make_shared<GPUQueue>(result, _async, _label);
}

std::shared_ptr<GPUCommandEncoder> GPUDevice::createCommandEncoder(
    std::optional<std::shared_ptr<GPUCommandEncoderDescriptor>> descriptor) {
  wgpu::CommandEncoderDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUCommandEncoderDescriptor");
  }
  auto result = _instance.CreateCommandEncoder(&desc);
  return std::make_shared<GPUCommandEncoder>(
      result,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

void GPUDevice::destroy() {
  _instance.Destroy();
  notifyDeviceLost(wgpu::DeviceLostReason::Destroyed, "device was destroyed");
}

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  wgpu::TextureDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUTextureDescriptor");
  }
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture, descriptor->label.value_or(""));
}

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderSourceWGSL wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  Convertor conv;
  if (!conv(wgsl_desc.code, descriptor->code) ||
      !conv(sm_desc.label, descriptor->label)) {
    return {};
  }
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    auto mod = _instance.CreateErrorShaderModule(
        &sm_desc, "The WGSL shader contains an illegal character '\\0'");
    return std::make_shared<GPUShaderModule>(mod, _async, sm_desc.label.data);
  }
  auto module = _instance.CreateShaderModule(&sm_desc);
  return std::make_shared<GPUShaderModule>(module, _async,
                                           descriptor->label.value_or(""));
}

std::shared_ptr<GPURenderPipeline> GPUDevice::createRenderPipeline(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  wgpu::RenderPipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPURenderPipelineDescriptor");
  }
  // assert(desc.fragment != nullptr && "Fragment state must not be null");
  auto renderPipeline = _instance.CreateRenderPipeline(&desc);
  return std::make_shared<GPURenderPipeline>(renderPipeline,
                                             descriptor->label.value_or(""));
}

std::shared_ptr<GPUBindGroup>
GPUDevice::createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor) {
  Convertor conv;
  wgpu::BindGroupDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.layout, descriptor->layout) ||
      !conv(desc.entries, desc.entryCount, descriptor->entries)) {
    throw std::runtime_error(
        "GPUBindGroup::createBindGroup(): Error with GPUBindGroupDescriptor");
  }
  auto bindGroup = _instance.CreateBindGroup(&desc);
  return std::make_shared<GPUBindGroup>(bindGroup,
                                        descriptor->label.value_or(""));
}

std::shared_ptr<GPUSampler> GPUDevice::createSampler(
    std::optional<std::shared_ptr<GPUSamplerDescriptor>> descriptor) {
  wgpu::SamplerDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createSampler(): Error with "
                             "GPUSamplerDescriptor");
  }
  auto sampler = _instance.CreateSampler(&desc);
  return std::make_shared<GPUSampler>(
      sampler,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

std::shared_ptr<GPUComputePipeline> GPUDevice::createComputePipeline(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                             "GPUComputePipelineDescriptor");
  }
  auto computePipeline = _instance.CreateComputePipeline(&desc);
  return std::make_shared<GPUComputePipeline>(computePipeline,
                                              descriptor->label.value_or(""));
}

std::shared_ptr<GPUQuerySet>
GPUDevice::createQuerySet(std::shared_ptr<GPUQuerySetDescriptor> descriptor) {
  wgpu::QuerySetDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createQuerySet(): Error with "
                             "GPUQuerySetDescriptor");
  }
  auto querySet = _instance.CreateQuerySet(&desc);
  return std::make_shared<GPUQuerySet>(querySet,
                                       descriptor->label.value_or(""));
}

std::shared_ptr<GPURenderBundleEncoder> GPUDevice::createRenderBundleEncoder(
    std::shared_ptr<GPURenderBundleEncoderDescriptor> descriptor) {
  Convertor conv;

  wgpu::RenderBundleEncoderDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.colorFormats, desc.colorFormatCount,
            descriptor->colorFormats) ||
      !conv(desc.depthStencilFormat, descriptor->depthStencilFormat) ||
      !conv(desc.sampleCount, descriptor->sampleCount) ||
      !conv(desc.depthReadOnly, descriptor->depthReadOnly) ||
      !conv(desc.stencilReadOnly, descriptor->stencilReadOnly)) {
    return {};
  }
  return std::make_shared<GPURenderBundleEncoder>(
      _instance.CreateRenderBundleEncoder(&desc),
      descriptor->label.value_or(""));
}

std::shared_ptr<GPUBindGroupLayout> GPUDevice::createBindGroupLayout(
    std::shared_ptr<GPUBindGroupLayoutDescriptor> descriptor) {
  Convertor conv;

  wgpu::BindGroupLayoutDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.entries, desc.entryCount, descriptor->entries)) {
    return {};
  }
  return std::make_shared<GPUBindGroupLayout>(
      _instance.CreateBindGroupLayout(&desc), descriptor->label.value_or(""));
}

std::shared_ptr<GPUPipelineLayout> GPUDevice::createPipelineLayout(
    std::shared_ptr<GPUPipelineLayoutDescriptor> descriptor) {
  Convertor conv;

  wgpu::PipelineLayoutDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.bindGroupLayouts, desc.bindGroupLayoutCount,
            descriptor->bindGroupLayouts)) {
    return {};
  }
  return std::make_shared<GPUPipelineLayout>(
      _instance.CreatePipelineLayout(&desc), descriptor->label.value_or(""));
}

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
    2.4f,                  // G
    1.0f / 1.055f,         // A
    0.055f / 1.055f,       // B
    1.0f / 12.92f,         // C
    0.04045f,              // D
    0.0f,                  // E
    0.0f,                  // F
};
// sRGB encode (linear -> encoded).
static const float kSrgbEncodeParams[7] = {
    1.0f / 2.4f,           // G
    1.055f,                // A
    0.0f,                  // B
    12.92f,                // C
    0.0031308f,            // D
    -0.055f,               // E
    0.0f,                  // F
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
// [Y, Cb, Cr, 1]. Same values the Apple NV12 path computes from the
// CVPixelBuffer; used for Android buffers that arrive as a *defined* biplanar
// format (where we split the planes and convert ourselves) rather than an
// opaque external-format AHB. Camera streams are limited-range BT.709 in the
// overwhelming majority of cases; full-range / BT.601 would need different
// coefficients (refine from the buffer's suggested range if it matters).
[[maybe_unused]] static const float kBT709LimitedToRgb[12] = {
    1.164383f, 0.000000f, 1.792741f, -0.972945f, //
    1.164383f, -0.213249f, -0.532909f, 0.301517f, //
    1.164383f, 2.112402f, 0.000000f, -1.133402f, //
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

std::shared_ptr<GPUExternalTexture> GPUDevice::importExternalTexture(
    std::shared_ptr<GPUExternalTextureDescriptor> descriptor) {
  if (!descriptor || !descriptor->source) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): descriptor.source (VideoFrame) "
        "is required");
  }
  const auto &source = descriptor->source;
  const auto &frame = source->handle();
  if (frame.handle == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): VideoFrame has been released");
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
  auto memory = _instance.ImportSharedTextureMemory(&memDesc);
  if (memory == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): ImportSharedTextureMemory "
        "returned null. Is 'shared-texture-memory-iosurface' enabled?");
  }

  // 2. Create the texture from the surface. We pass the right format
  //    explicitly so Dawn picks the multi-planar variant on NV12.
  bool isYuv = frame.pixelFormat == VideoPixelFormat::NV12;
  auto texture = memory.CreateTexture();
  if (texture == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): CreateTexture returned null");
  }

  // 3. Begin access on the underlying memory. The matching EndAccess runs in
  //    the GPUExternalTexture destructor.
  wgpu::SharedTextureMemoryBeginAccessDescriptor begin{};
  begin.initialized = true;
  begin.concurrentRead = false;
  if (!memory.BeginAccess(texture, &begin)) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): BeginAccess failed");
  }

  // 4. Build plane views. For NV12 we need plane0 = R8 luma and plane1 = RG8
  //    chroma; for BGRA we only set plane0.
  wgpu::TextureView plane0;
  wgpu::TextureView plane1;
  {
    wgpu::TextureViewDescriptor v{};
    v.aspect = isYuv ? wgpu::TextureAspect::Plane0Only
                     : wgpu::TextureAspect::All;
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

  auto external = _instance.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    wgpu::SharedTextureMemoryEndAccessState state{};
    (void)memory.EndAccess(texture, &state);
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): CreateExternalTexture returned "
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
  auto memory = _instance.ImportSharedTextureMemory(&memDesc);
  if (memory == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): ImportSharedTextureMemory "
        "returned null. Is 'shared-texture-memory-ahardware-buffer' enabled?");
  }

  // 2. Create the texture. No descriptor: Dawn picks the right format
  //    (OpaqueYCbCrAndroid for YUV, R8 / RGBA8 / ... for color AHBs).
  auto texture = memory.CreateTexture();
  if (texture == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): CreateTexture returned null");
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
        "GPUDevice::importExternalTexture(): BeginAccess failed");
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
  const bool isBiplanar =
      frame.pixelFormat == VideoPixelFormat::NV12 &&
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

  auto external = _instance.CreateExternalTexture(&extDesc);
  if (external == nullptr) {
    wgpu::SharedTextureMemoryEndAccessState state{};
    (void)memory.EndAccess(texture, &state);
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): CreateExternalTexture returned "
        "null");
  }

  return std::make_shared<GPUExternalTexture>(
      std::move(external), std::move(memory), std::move(texture),
      std::move(descriptor->source), std::move(label));
#else
  throw std::runtime_error(
      "GPUDevice::importExternalTexture(): not yet implemented on this "
      "platform");
#endif
}

std::shared_ptr<VideoFrame>
GPUDevice::createVideoFrameFromNativeBuffer(uint64_t pointer) {
  auto platformContext = PlatformContext::global();
  if (!platformContext) {
    throw std::runtime_error(
        "GPUDevice::createVideoFrameFromNativeBuffer(): PlatformContext is "
        "not initialized");
  }
  auto handle =
      platformContext->wrapNativeBuffer(reinterpret_cast<void *>(pointer));
  return std::make_shared<VideoFrame>(std::move(handle));
}

std::shared_ptr<GPUSharedTextureMemory> GPUDevice::importSharedTextureMemory(
    std::shared_ptr<GPUSharedTextureMemoryDescriptor> descriptor) {
  if (!descriptor || descriptor->handle == nullptr) {
    throw std::runtime_error("GPUDevice::importSharedTextureMemory(): handle "
                             "must be a non-null native pointer");
  }

  wgpu::SharedTextureMemoryDescriptor desc{};
  std::string label = descriptor->label.value_or("");
  if (!label.empty()) {
    desc.label = wgpu::StringView(label.c_str(), label.size());
  }

#if defined(__APPLE__)
  wgpu::SharedTextureMemoryIOSurfaceDescriptor platformDesc{};
  platformDesc.ioSurface = descriptor->handle;
  // Default off: enabling it propagates StorageBinding into properties.usage,
  // which then forces memory.createTexture() (no-descriptor form) to validate
  // the format against storage capabilities. bgra8unorm (the standard
  // CVPixelBuffer format) only supports storage when the device opts into the
  // bgra8unorm-storage feature, so unconditionally setting this here breaks
  // the common sample-only case.
  platformDesc.allowStorageBinding = false;
  desc.nextInChain = &platformDesc;
#elif defined(__ANDROID__)
  wgpu::SharedTextureMemoryAHardwareBufferDescriptor platformDesc{};
  platformDesc.handle = descriptor->handle;
  desc.nextInChain = &platformDesc;
#else
  throw std::runtime_error(
      "GPUDevice::importSharedTextureMemory(): unsupported platform");
#endif

  auto memory = _instance.ImportSharedTextureMemory(&desc);
  if (memory == nullptr) {
    throw std::runtime_error("GPUDevice::importSharedTextureMemory(): "
                             "ImportSharedTextureMemory returned null - is the "
                             "'shared-texture-memory-iosurface' (Apple) or "
                             "'shared-texture-memory-ahardware-buffer' "
                             "(Android) feature enabled on the device?");
  }
  return std::make_shared<GPUSharedTextureMemory>(std::move(memory),
                                                  std::move(label));
}

async::AsyncTaskHandle GPUDevice::createComputePipelineAsync(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                             "GPUComputePipelineDescriptor");
  }

  auto label = std::string(
      descriptor->label.has_value() ? descriptor->label.value() : "");
  auto pipelineHolder = std::make_shared<GPUComputePipeline>(nullptr, label);

  return _async->postTask([device = _instance, desc, descriptor,
                           pipelineHolder](
                              const async::AsyncTaskHandle::ResolveFunction
                                  &resolve,
                              const async::AsyncTaskHandle::RejectFunction
                                  &reject) {
    (void)descriptor;
    device.CreateComputePipelineAsync(
        &desc, wgpu::CallbackMode::AllowProcessEvents,
        [pipelineHolder, resolve,
         reject](wgpu::CreatePipelineAsyncStatus status,
                 wgpu::ComputePipeline pipeline, wgpu::StringView msg) {
          if (status == wgpu::CreatePipelineAsyncStatus::Success && pipeline) {
            pipelineHolder->_instance = pipeline;
            resolve([pipelineHolder](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUComputePipeline>>::toJSI(
                  runtime, pipelineHolder);
            });
          } else {
            std::string error =
                msg.length ? std::string(msg.data, msg.length)
                           : "Failed to create compute pipeline";
            reject(std::move(error));
          }
        });
  });
}

async::AsyncTaskHandle GPUDevice::createRenderPipelineAsync(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  wgpu::RenderPipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error(
        "GPUDevice::createRenderPipelineAsync(): Error with "
        "GPURenderPipelineDescriptor");
  }

  auto label = std::string(
      descriptor->label.has_value() ? descriptor->label.value() : "");
  auto pipelineHolder = std::make_shared<GPURenderPipeline>(nullptr, label);

  return _async->postTask([device = _instance, desc, descriptor,
                           pipelineHolder](
                              const async::AsyncTaskHandle::ResolveFunction
                                  &resolve,
                              const async::AsyncTaskHandle::RejectFunction
                                  &reject) {
    (void)descriptor;
    device.CreateRenderPipelineAsync(
        &desc, wgpu::CallbackMode::AllowProcessEvents,
        [pipelineHolder, resolve,
         reject](wgpu::CreatePipelineAsyncStatus status,
                 wgpu::RenderPipeline pipeline, wgpu::StringView msg) {
          if (status == wgpu::CreatePipelineAsyncStatus::Success && pipeline) {
            pipelineHolder->_instance = pipeline;
            resolve([pipelineHolder](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPURenderPipeline>>::toJSI(
                  runtime, pipelineHolder);
            });
          } else {
            std::string error =
                msg.length ? std::string(msg.data, msg.length)
                           : "Failed to create render pipeline";
            reject(std::move(error));
          }
        });
  });
}

void GPUDevice::pushErrorScope(wgpu::ErrorFilter filter) {
  _instance.PushErrorScope(filter);
}

async::AsyncTaskHandle GPUDevice::popErrorScope() {
  auto device = _instance;

  return _async->postTask([device](const async::AsyncTaskHandle::ResolveFunction
                                       &resolve,
                                   const async::AsyncTaskHandle::RejectFunction
                                       &reject) {
    device.PopErrorScope(
        wgpu::CallbackMode::AllowProcessEvents,
        [resolve, reject](wgpu::PopErrorScopeStatus status,
                          wgpu::ErrorType type, wgpu::StringView message) {
          if (status == wgpu::PopErrorScopeStatus::Error ||
              status == wgpu::PopErrorScopeStatus::CallbackCancelled) {
            reject("PopErrorScope failed");
            return;
          }

          std::string messageString =
              message.length ? std::string(message.data, message.length) : "";

          switch (type) {
          case wgpu::ErrorType::NoError:
            resolve([](jsi::Runtime &runtime) mutable {
              return jsi::Value::null();
            });
            break;
          case wgpu::ErrorType::Validation: {
            auto error = std::make_shared<GPUValidationError>(messageString);
            resolve([error](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUValidationError>>::toJSI(
                  runtime, error);
            });
            break;
          }
          case wgpu::ErrorType::OutOfMemory: {
            auto error = std::make_shared<GPUOutOfMemoryError>(messageString);
            resolve([error](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUOutOfMemoryError>>::toJSI(
                  runtime, error);
            });
            break;
          }
          case wgpu::ErrorType::Internal:
          case wgpu::ErrorType::Unknown: {
            auto error = std::make_shared<GPUInternalError>(messageString);
            resolve([error](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUInternalError>>::toJSI(
                  runtime, error);
            });
            break;
          }
          default:
            reject("Unhandled GPU error type");
            return;
          }
        });
  });
}

std::unordered_set<std::string> GPUDevice::getFeatures() {
  wgpu::SupportedFeatures supportedFeatures;
  _instance.GetFeatures(&supportedFeatures);
  std::unordered_set<std::string> result;
  std::unordered_set<wgpu::FeatureName> enabled;
  for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
    auto feature = supportedFeatures.features[i];
    enabled.insert(feature);
    std::string name;
    convertEnumToJSUnion(feature, &name);
    result.insert(name);
  }
  maybeSynthesizeRnSharedTextureMemoryFeature(enabled, result);
  return result;
}

async::AsyncTaskHandle GPUDevice::getLost() {
  if (_lostHandle.has_value()) {
    return *_lostHandle;
  }

  if (_lostSettled && _lostInfo) {
    return _async->postTask(
        [info = _lostInfo](
            const async::AsyncTaskHandle::ResolveFunction &resolve,
            const async::AsyncTaskHandle::RejectFunction & /*reject*/) {
          resolve([info](jsi::Runtime &runtime) mutable {
            return JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
                runtime, info);
          });
        },
        false);
  }

  auto handle = _async->postTask(
      [this](const async::AsyncTaskHandle::ResolveFunction &resolve,
             const async::AsyncTaskHandle::RejectFunction & /*reject*/) {
        if (_lostSettled && _lostInfo) {
          resolve([info = _lostInfo](jsi::Runtime &runtime) mutable {
            return JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
                runtime, info);
          });
          return;
        }

        _lostResolve = resolve;
      },
      false);

  _lostHandle = handle;
  return handle;
}
void GPUDevice::addEventListener(std::string type, jsi::Function callback) {
  auto funcPtr = std::make_shared<jsi::Function>(std::move(callback));
  _eventListeners[type].push_back(funcPtr);
}

void GPUDevice::removeEventListener(std::string type, jsi::Function callback) {
  // Note: Since jsi::Function doesn't support equality comparison,
  // we cannot reliably remove a specific listener. This is a no-op.
  // Most use cases (like BabylonJS) only need addEventListener to work.
  (void)type;
  (void)callback;
}

void GPUDevice::notifyUncapturedError(wgpu::ErrorType type,
                                      std::string message) {
  auto it = _eventListeners.find("uncapturederror");
  if (it == _eventListeners.end() || it->second.empty()) {
    return;
  }

  auto runtime = getCreationRuntime();
  if (runtime == nullptr) {
    return;
  }

  // Create the appropriate error object based on type
  GPUErrorVariant error;
  switch (type) {
  case wgpu::ErrorType::Validation:
    error = std::make_shared<GPUValidationError>(message);
    break;
  case wgpu::ErrorType::OutOfMemory:
    error = std::make_shared<GPUOutOfMemoryError>(message);
    break;
  case wgpu::ErrorType::Internal:
  case wgpu::ErrorType::Unknown:
  default:
    error = std::make_shared<GPUInternalError>(message);
    break;
  }

  // Create the event object
  auto event = std::make_shared<GPUUncapturedErrorEvent>(std::move(error));
  auto eventValue =
      JSIConverter<std::shared_ptr<GPUUncapturedErrorEvent>>::toJSI(*runtime,
                                                                    event);

  // Call all registered listeners
  for (const auto &listener : it->second) {
    try {
      listener->call(*runtime, eventValue);
    } catch (const std::exception &e) {
      // Log but don't throw - we don't want one listener to break others
      fprintf(stderr, "Error in uncapturederror listener: %s\n", e.what());
    }
  }
}

} // namespace rnwgpu
