#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

// A stand-in for the WebGPU C functions SurfaceRegistry.h calls, linked in
// place of Dawn. It keeps what a surface, a device and a queue were asked to
// do, and records as an error every call Dawn would reject.
namespace fake_webgpu {

struct Copy {
  int sourceTexture;
  int destinationTexture;
  // The surface whose swapchain made the destination; 0 for a device texture.
  int destinationSurface;
  uint32_t width;
  uint32_t height;
};

// Forgets every record; objects still alive keep working.
void reset();

wgpu::Device createDevice();
wgpu::Surface createSurface();

int textureId(const wgpu::Texture &texture);
int surfaceId(const wgpu::Surface &surface);
// The surface whose swapchain made the texture; 0 for a device texture.
int surfaceOf(const wgpu::Texture &texture);

int configureCount(const wgpu::Surface &surface);
int presentCount(const wgpu::Surface &surface);
bool isConfigured(const wgpu::Surface &surface);
WGPUSurfaceConfiguration lastConfiguration(const wgpu::Surface &surface);
// The status the surface's next GetCurrentTexture answers with, once, instead
// of a texture.
void failNextAcquire(const wgpu::Surface &surface,
                     wgpu::SurfaceGetCurrentTextureStatus status);

// Every copy submitted to a queue, in order.
std::vector<Copy> submittedCopies();
// Every call Dawn would have rejected.
std::vector<std::string> errors();
// The fake objects not yet released.
int liveObjects();

} // namespace fake_webgpu
