#include "FakeWebGPU.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Ledger {
  int nextId = 1;
  int liveObjects = 0;
  std::vector<fake_webgpu::Copy> copies;
  std::vector<std::string> errors;
};

Ledger &ledger() {
  static Ledger instance;
  return instance;
}

void reject(std::string call) { ledger().errors.push_back(std::move(call)); }

} // namespace

namespace fake_webgpu::detail {

struct RefCounted {
  RefCounted() : id(ledger().nextId++) { ledger().liveObjects++; }
  RefCounted(const RefCounted &) = delete;
  RefCounted &operator=(const RefCounted &) = delete;
  virtual ~RefCounted() { ledger().liveObjects--; }
  int id;
  int references = 1;
};

template <typename T> void addReference(T *object) { object->references++; }

template <typename T> void releaseReference(T *object) {
  if (--object->references == 0) {
    delete object;
  }
}

} // namespace fake_webgpu::detail

using fake_webgpu::detail::addReference;
using fake_webgpu::detail::RefCounted;
using fake_webgpu::detail::releaseReference;

struct WGPUInstanceImpl : RefCounted {};

struct WGPUQueueImpl : RefCounted {};

struct WGPUDeviceImpl : RefCounted {};

struct WGPUTextureImpl : RefCounted {
  uint32_t width = 0;
  uint32_t height = 0;
  // The surface whose swapchain made the texture; 0 for a device texture.
  int surfaceId = 0;
  // A surface destroys the texture it hands out when it presents it, is
  // configured again or is destroyed (Dawn's SwapChain.cpp).
  bool destroyed = false;
};

struct WGPUSurfaceImpl : RefCounted {
  ~WGPUSurfaceImpl() override { dropCurrentTexture(); }
  void dropCurrentTexture() {
    if (current != nullptr) {
      current->destroyed = true;
      releaseReference(current);
      current = nullptr;
    }
  }
  bool configured = false;
  WGPUSurfaceConfiguration configuration = {};
  // The texture acquired since the last present, configure or unconfigure.
  WGPUTextureImpl *current = nullptr;
  std::optional<WGPUSurfaceGetCurrentTextureStatus> nextFailure;
  int configureCount = 0;
  int presentCount = 0;
};

// The textures a command encoder or buffer uses, each referenced until the
// commands are released.
struct UsedTextures {
  UsedTextures() = default;
  UsedTextures(const UsedTextures &) = delete;
  UsedTextures &operator=(const UsedTextures &) = delete;
  ~UsedTextures() {
    for (auto *texture : textures) {
      releaseReference(texture);
    }
  }
  void add(WGPUTextureImpl *texture) {
    addReference(texture);
    textures.push_back(texture);
  }
  std::vector<WGPUTextureImpl *> textures;
};

struct WGPUCommandEncoderImpl : RefCounted {
  std::vector<fake_webgpu::Copy> copies;
  UsedTextures used;
};

struct WGPUCommandBufferImpl : RefCounted {
  std::vector<fake_webgpu::Copy> copies;
  UsedTextures used;
};

extern "C" {

void wgpuInstanceAddRef(WGPUInstance instance) { addReference(instance); }
void wgpuInstanceRelease(WGPUInstance instance) { releaseReference(instance); }

void wgpuDeviceAddRef(WGPUDevice device) { addReference(device); }
void wgpuDeviceRelease(WGPUDevice device) { releaseReference(device); }

WGPUQueue wgpuDeviceGetQueue(WGPUDevice) { return new WGPUQueueImpl(); }
void wgpuQueueRelease(WGPUQueue queue) { releaseReference(queue); }

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice,
                                    WGPUTextureDescriptor const *descriptor) {
  auto *texture = new WGPUTextureImpl();
  texture->width = descriptor->size.width;
  texture->height = descriptor->size.height;
  return texture;
}

void wgpuTextureAddRef(WGPUTexture texture) { addReference(texture); }
void wgpuTextureRelease(WGPUTexture texture) { releaseReference(texture); }
uint32_t wgpuTextureGetWidth(WGPUTexture texture) { return texture->width; }
uint32_t wgpuTextureGetHeight(WGPUTexture texture) { return texture->height; }

WGPUCommandEncoder
wgpuDeviceCreateCommandEncoder(WGPUDevice,
                               WGPUCommandEncoderDescriptor const *) {
  return new WGPUCommandEncoderImpl();
}

void wgpuCommandEncoderCopyTextureToTexture(
    WGPUCommandEncoder encoder, WGPUTexelCopyTextureInfo const *source,
    WGPUTexelCopyTextureInfo const *destination, WGPUExtent3D const *copySize) {
  encoder->copies.push_back({source->texture->id, destination->texture->id,
                             destination->texture->surfaceId, copySize->width,
                             copySize->height});
  encoder->used.add(source->texture);
  encoder->used.add(destination->texture);
}

WGPUCommandBuffer
wgpuCommandEncoderFinish(WGPUCommandEncoder encoder,
                         WGPUCommandBufferDescriptor const *) {
  auto *commands = new WGPUCommandBufferImpl();
  commands->copies = std::move(encoder->copies);
  std::swap(commands->used.textures, encoder->used.textures);
  return commands;
}

void wgpuCommandEncoderRelease(WGPUCommandEncoder encoder) {
  releaseReference(encoder);
}

void wgpuCommandBufferRelease(WGPUCommandBuffer commands) {
  releaseReference(commands);
}

void wgpuQueueSubmit(WGPUQueue, size_t commandCount,
                     WGPUCommandBuffer const *commands) {
  for (size_t index = 0; index < commandCount; index++) {
    for (const auto *texture : commands[index]->used.textures) {
      if (texture->destroyed) {
        reject("Queue.Submit with a destroyed texture");
      }
    }
    for (const auto &copy : commands[index]->copies) {
      ledger().copies.push_back(copy);
    }
  }
}

void wgpuSurfaceConfigure(WGPUSurface surface,
                          WGPUSurfaceConfiguration const *configuration) {
  if (configuration->device == nullptr) {
    reject("Surface.Configure without a device");
    return;
  }
  surface->dropCurrentTexture();
  surface->configured = true;
  surface->configuration = *configuration;
  surface->configuration.nextInChain = nullptr;
  surface->configuration.viewFormats = nullptr;
  surface->configureCount++;
}

void wgpuSurfaceUnconfigure(WGPUSurface surface) {
  if (!surface->configured) {
    reject("Surface.Unconfigure on a surface that is not configured");
    return;
  }
  surface->dropCurrentTexture();
  surface->configured = false;
}

void wgpuSurfaceGetCurrentTexture(WGPUSurface surface,
                                  WGPUSurfaceTexture *surfaceTexture) {
  surfaceTexture->texture = nullptr;
  surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_Error;
  if (!surface->configured) {
    reject("Surface.GetCurrentTexture on a surface that is not configured");
    return;
  }
  if (surface->nextFailure.has_value()) {
    surfaceTexture->status = *surface->nextFailure;
    surface->nextFailure.reset();
    return;
  }
  if (surface->current == nullptr) {
    surface->current = new WGPUTextureImpl();
    surface->current->width = surface->configuration.width;
    surface->current->height = surface->configuration.height;
    surface->current->surfaceId = surface->id;
  }
  addReference(surface->current);
  surfaceTexture->texture = surface->current;
  surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal;
}

WGPUStatus wgpuSurfacePresent(WGPUSurface surface) {
  if (!surface->configured) {
    reject("Surface.Present on a surface that is not configured");
    return WGPUStatus_Error;
  }
  if (surface->current == nullptr) {
    reject("Surface.Present without an acquired texture");
    return WGPUStatus_Error;
  }
  surface->dropCurrentTexture();
  surface->presentCount++;
  return WGPUStatus_Success;
}

void wgpuSurfaceAddRef(WGPUSurface surface) { addReference(surface); }
void wgpuSurfaceRelease(WGPUSurface surface) { releaseReference(surface); }

} // extern "C"

namespace fake_webgpu {

void reset() {
  ledger().copies.clear();
  ledger().errors.clear();
}

wgpu::Device createDevice() {
  return wgpu::Device::Acquire(new WGPUDeviceImpl());
}

wgpu::Surface createSurface() {
  return wgpu::Surface::Acquire(new WGPUSurfaceImpl());
}

int textureId(const wgpu::Texture &texture) { return texture.Get()->id; }

int surfaceId(const wgpu::Surface &surface) { return surface.Get()->id; }

int surfaceOf(const wgpu::Texture &texture) { return texture.Get()->surfaceId; }

int configureCount(const wgpu::Surface &surface) {
  return surface.Get()->configureCount;
}

int presentCount(const wgpu::Surface &surface) {
  return surface.Get()->presentCount;
}

bool isConfigured(const wgpu::Surface &surface) {
  return surface.Get()->configured;
}

WGPUSurfaceConfiguration lastConfiguration(const wgpu::Surface &surface) {
  return surface.Get()->configuration;
}

void failNextAcquire(const wgpu::Surface &surface,
                     wgpu::SurfaceGetCurrentTextureStatus status) {
  surface.Get()->nextFailure =
      static_cast<WGPUSurfaceGetCurrentTextureStatus>(status);
}

std::vector<Copy> submittedCopies() { return ledger().copies; }

std::vector<std::string> errors() { return ledger().errors; }

int liveObjects() { return ledger().liveObjects; }

} // namespace fake_webgpu
