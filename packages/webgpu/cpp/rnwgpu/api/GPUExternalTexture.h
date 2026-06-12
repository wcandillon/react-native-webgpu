#pragma once

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Unions.h"

#include "NativeObject.h"
#include "VideoFrame.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct GPUExternalTextureDescriptor;

class GPUExternalTexture : public NativeObject<GPUExternalTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUExternalTexture";

  // Import a VideoFrame (via descriptor.source) as a GPUExternalTexture on
  // `device`: imports the native surface as SharedTextureMemory, begins access,
  // and wraps the resulting wgpu::ExternalTexture together with the resources
  // whose lifetime it owns. The matching EndAccess runs in destroy() / the
  // destructor. Defined in GPUExternalTexture.cpp.
  static std::shared_ptr<GPUExternalTexture>
  Create(wgpu::Device device,
         std::shared_ptr<GPUExternalTextureDescriptor> descriptor);

  // Construct from an already-built wgpu::ExternalTexture plus the underlying
  // shared-memory resources we need to keep alive. The wrapper takes ownership
  // of the SharedTextureMemory + Texture and calls EndAccess on destruction so
  // the producer (e.g. AVPlayer) can reclaim the IOSurface.
  GPUExternalTexture(wgpu::ExternalTexture instance,
                     wgpu::SharedTextureMemory memory, wgpu::Texture texture,
                     std::shared_ptr<VideoFrame> source, std::string label,
                     std::array<float, 12> yuvToRgbMatrix)
      : NativeObject(CLASS_NAME), _instance(std::move(instance)),
        _memory(std::move(memory)), _texture(std::move(texture)),
        _source(std::move(source)), _label(std::move(label)),
        _yuvToRgbMatrix(yuvToRgbMatrix) {}

  ~GPUExternalTexture() override { destroy(); }

public:
  std::string getBrand() { return CLASS_NAME; }

  // End the shared-memory access window and release the underlying resources.
  // Idempotent: safe to call more than once, and the destructor calls it as a
  // garbage-collection fallback. Call it right after the queue.submit() that
  // sampled this texture (never before): a GPUExternalTexture's access window
  // is owned by this wrapper's lifetime, not by submit, so without an explicit
  // destroy() the producer's surface (e.g. an AVPlayer IOSurface) stays claimed
  // until GC runs. EndAccess is the designed post-submit call: Dawn keeps the
  // texture alive for in-flight GPU work via the fences it returns.
  void destroy() {
    if (_memory && _texture) {
      wgpu::SharedTextureMemoryEndAccessState state{};
      (void)_memory.EndAccess(_texture, &state);
    }
    _texture = nullptr;
    _memory = nullptr;
  }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  // Non-spec extension. A 3x4 row-major matrix mapping the *sampled* texel
  // [c.r, c.g, c.b, 1] to gamma-encoded R'G'B'. When Dawn's sampler already
  // produces RGB (Apple's biplanar path, RGBA surfaces), this is the identity
  // passthrough; on the Android opaque-YCbCr path the sample comes back as raw
  // [Y, Cb, Cr] (Dawn hard-codes an RGB_IDENTITY Vulkan conversion, see
  // crbug.com/497675620) and this matrix is derived from the driver's
  // suggested YCbCr model + range for the buffer. Shaders can therefore apply
  // it unconditionally after textureSampleBaseClampToEdge.
  std::vector<double> getYuvToRgbMatrix() {
    return std::vector<double>(_yuvToRgbMatrix.begin(), _yuvToRgbMatrix.end());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUExternalTexture::getBrand);
    installGetterSetter(runtime, prototype, "label",
                        &GPUExternalTexture::getLabel,
                        &GPUExternalTexture::setLabel);
    installGetter(runtime, prototype, "yuvToRgbMatrix",
                  &GPUExternalTexture::getYuvToRgbMatrix);
    installMethod(runtime, prototype, "destroy", &GPUExternalTexture::destroy);
  }

  inline const wgpu::ExternalTexture get() { return _instance; }

private:
  wgpu::ExternalTexture _instance;
  wgpu::SharedTextureMemory _memory;
  wgpu::Texture _texture;
  std::shared_ptr<VideoFrame> _source;
  std::string _label;
  std::array<float, 12> _yuvToRgbMatrix;
};

} // namespace rnwgpu
