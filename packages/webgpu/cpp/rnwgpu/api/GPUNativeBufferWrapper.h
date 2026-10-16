#pragma once

#include <memory>
#include <string>

#include <jsi/jsi.h>

#include "webgpu/webgpu_cpp.h"

#include "VideoFrame.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// What GPUNativeBufferTexture and GPUNativeBufferExternalTexture share:
// the import of a native buffer, done once, the access window around each use
// and the release. The counterpart of GPUHardwareBufferWrapper in Dawn's
// Android layer (tools/android/webgpu), whose JNI side owns the same objects:
// the shared texture memory, the texture, the buffer reference.
//
// A decoder or a camera renders into the same few buffers in turn: importing
// each one once and bracketing every frame with beginAccess() / endAccess()
// is what Dawn's layer does, and saves the import (and the pipeline recreation
// behind a new external texture) that importExternalTexture() pays per frame.
class NativeBufferAccess {
public:
  NativeBufferAccess(const char *className, wgpu::Device device,
                     wgpu::SharedTextureMemory memory, wgpu::Texture texture,
                     std::shared_ptr<VideoFrame> source, std::string label);
  ~NativeBufferAccess();

  NativeBufferAccess(const NativeBufferAccess &) = delete;
  NativeBufferAccess &operator=(const NativeBufferAccess &) = delete;

  // beginAccess(fences?): begins an access to the buffer's texture, after the
  // GPU waited for the optional GPUSharedFenceState[] (the producer's fence
  // for the frame: NativeVideoFrame.acquireFenceFd imported with
  // importSharedFence, or the fences a native module hands over). The frame
  // is taken as initialized, as Dawn's layer does. Throws (after a validation
  // error) on a destroyed wrapper or fence, and if Dawn rejects the access.
  void beginAccess(jsi::Runtime &runtime, const jsi::Value *args, size_t count);

  // endAccess(): ends the access and returns the one fence that signals once
  // the GPU work of the access is done, as { fence, signaledValue }, or null
  // when there is none. Hand it to the buffer's producer before it reuses the
  // buffer, or destroy it.
  jsi::Value endAccess(jsi::Runtime &runtime);

  // Releases the import now rather than when the JS object is garbage
  // collected: the texture, the memory and the frame that keeps the buffer
  // alive. An access still open is a validation error; it is ended (its fence
  // is lost) so the producer gets its buffer back. A later use is a validation
  // error and a throw.
  void destroy();

  bool destroyed() const { return _memory == nullptr; }

  const std::string &label() const { return _label; }
  void setLabel(const std::string &label);

private:
  void release(bool reportOpenAccess);
  void injectDestroyedError(const char *method);

  const char *_className;
  wgpu::Device _device;
  wgpu::SharedTextureMemory _memory;
  wgpu::Texture _texture;
  // Keeps the native buffer (and, on Android, its acquire fence) alive for as
  // long as the import is.
  std::shared_ptr<VideoFrame> _source;
  std::string _label;
  bool _accessOpen = false;
};

} // namespace rnwgpu
