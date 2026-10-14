#pragma once

#include <android/hardware_buffer.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/rect.h>
#include <dlfcn.h>
#include <unistd.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include "SurfaceRegistry.h"

namespace rnwgpu {

// The NDK's ASurfaceControl API (android/surface_control.h, API 29), resolved
// at run time: the library runs from API 26, so the symbols cannot be linked
// directly. The handle types are opaque; the enum parameters are ints, which
// is their C ABI.
struct SurfaceControlFunctions {
  struct Control;
  struct Transaction;
  struct Stats;
  using OnComplete = void (*)(void *context, Stats *stats);

  Control *(*createFromWindow)(ANativeWindow *, const char *) = nullptr;
  void (*release)(Control *) = nullptr;
  Transaction *(*transactionCreate)() = nullptr;
  void (*transactionDelete)(Transaction *) = nullptr;
  void (*transactionApply)(Transaction *) = nullptr;
  void (*setOnComplete)(Transaction *, void *, OnComplete) = nullptr;
  int (*getPreviousReleaseFenceFd)(Stats *, Control *) = nullptr;
  void (*setVisibility)(Transaction *, Control *, int) = nullptr;
  void (*setBuffer)(Transaction *, Control *, AHardwareBuffer *, int) = nullptr;
  void (*setGeometry)(Transaction *, Control *, const ARect &, const ARect &,
                      int32_t) = nullptr;
  void (*setBufferTransparency)(Transaction *, Control *, int) = nullptr;

  // ASurfaceTransactionVisibility / ASurfaceTransactionTransparency values.
  static constexpr int kVisibilityShow = 1;
  static constexpr int kTransparencyTranslucent = 1;
  static constexpr int kTransparencyOpaque = 2;

  bool available() const {
    return createFromWindow && release && transactionCreate &&
           transactionDelete && transactionApply && setOnComplete &&
           getPreviousReleaseFenceFd && setVisibility && setBuffer &&
           setGeometry && setBufferTransparency;
  }

  static const SurfaceControlFunctions &get() {
    static const SurfaceControlFunctions functions = load();
    return functions;
  }

private:
  static SurfaceControlFunctions load() {
    SurfaceControlFunctions f;
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) {
      return f;
    }
    auto resolve = [lib](auto &slot, const char *name) {
      slot = reinterpret_cast<std::decay_t<decltype(slot)>>(dlsym(lib, name));
    };
    resolve(f.createFromWindow, "ASurfaceControl_createFromWindow");
    resolve(f.release, "ASurfaceControl_release");
    resolve(f.transactionCreate, "ASurfaceTransaction_create");
    resolve(f.transactionDelete, "ASurfaceTransaction_delete");
    resolve(f.transactionApply, "ASurfaceTransaction_apply");
    resolve(f.setOnComplete, "ASurfaceTransaction_setOnComplete");
    resolve(f.getPreviousReleaseFenceFd,
            "ASurfaceTransactionStats_getPreviousReleaseFenceFd");
    resolve(f.setVisibility, "ASurfaceTransaction_setVisibility");
    resolve(f.setBuffer, "ASurfaceTransaction_setBuffer");
    resolve(f.setGeometry, "ASurfaceTransaction_setGeometry");
    resolve(f.setBufferTransparency,
            "ASurfaceTransaction_setBufferTransparency");
    return f;
  }
};

// Presents the pool's buffers through a child surface of the view's window
// (WebGPUSurfaceControlView). Each finished buffer is posted to the compositor
// with the sync fd of its render-complete fence as the acquire fence, so no
// thread waits on the CPU; when the next frame replaces it, the compositor
// reports the fence it signals once it is done reading the buffer, and the
// pool makes the next render into that slot wait on it. Both hand-offs are
// GPU fences.
//
// Transactions are posted from the rendering thread; the compositor's
// completion callbacks arrive on a binder thread. The state they share is
// behind `_shared`, which outlives the presenter for the callbacks still in
// flight after it is closed.
class SurfaceControlPresenter : public PoolPresenter {
public:
  // Takes ownership of the window reference (released on failure too).
  // Returns null when the platform lacks the API (before API 29) or the
  // surface cannot be created.
  static std::shared_ptr<SurfaceControlPresenter>
  create(ANativeWindow *window, int viewWidth, int viewHeight, bool opaque) {
    const auto &f = SurfaceControlFunctions::get();
    if (window == nullptr) {
      return nullptr;
    }
    if (!f.available()) {
      ANativeWindow_release(window);
      return nullptr;
    }
    auto *control = f.createFromWindow(window, "WebGPUCanvas");
    if (control == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, "WebGPUSurfaceControlView",
                          "ASurfaceControl_createFromWindow failed");
      ANativeWindow_release(window);
      return nullptr;
    }
    auto presenter = std::shared_ptr<SurfaceControlPresenter>(
        new SurfaceControlPresenter(f, window, control));
    presenter->_shared->viewWidth = viewWidth;
    presenter->_shared->viewHeight = viewHeight;
    presenter->_shared->opaque = opaque;
    return presenter;
  }

  ~SurfaceControlPresenter() override {
    {
      std::lock_guard<std::mutex> lock(_shared->mutex);
      _shared->closed = true;
      _shared->onRelease = nullptr;
      if (_shared->control != nullptr) {
        _f.release(_shared->control);
        _shared->control = nullptr;
      }
    }
    ANativeWindow_release(_window);
  }

  void setReleaseCallback(ReleaseCallback callback) override {
    std::lock_guard<std::mutex> lock(_shared->mutex);
    _shared->onRelease = std::move(callback);
  }

  // The view's size in pixels: the buffer is scaled to it, so a buffer that
  // lags the view size (mid-resize) shows stretched rather than blank.
  void resize(int viewWidth, int viewHeight) {
    std::lock_guard<std::mutex> lock(_shared->mutex);
    _shared->viewWidth = viewWidth;
    _shared->viewHeight = viewHeight;
  }

  void setOpaque(bool opaque) {
    std::lock_guard<std::mutex> lock(_shared->mutex);
    _shared->opaque = opaque;
  }

  void present(AHardwareBuffer *buffer, int acquireFenceFd,
               uint64_t token) override {
    std::lock_guard<std::mutex> lock(_shared->mutex);
    if (_shared->closed || _shared->control == nullptr || buffer == nullptr) {
      if (acquireFenceFd >= 0) {
        ::close(acquireFenceFd);
      }
      return;
    }
    AHardwareBuffer_Desc desc{};
    AHardwareBuffer_describe(buffer, &desc);

    auto *transaction = _f.transactionCreate();
    // The compositor takes ownership of the acquire fence.
    _f.setBuffer(transaction, _shared->control, buffer, acquireFenceFd);
    _f.setVisibility(transaction, _shared->control,
                     SurfaceControlFunctions::kVisibilityShow);
    _f.setBufferTransparency(
        transaction, _shared->control,
        _shared->opaque ? SurfaceControlFunctions::kTransparencyOpaque
                        : SurfaceControlFunctions::kTransparencyTranslucent);
    ARect source{0, 0, static_cast<int32_t>(desc.width),
                 static_cast<int32_t>(desc.height)};
    ARect destination{0, 0, _shared->viewWidth, _shared->viewHeight};
    _f.setGeometry(transaction, _shared->control, source, destination,
                   /* ANATIVEWINDOW_TRANSFORM_IDENTITY */ 0);
    // When this transaction completes, the buffer it replaces is released:
    // the callback reports that buffer's slot back to the pool.
    auto *completion =
        new Completion{_shared, _shared->lastToken, _shared->hasLastToken};
    _f.setOnComplete(transaction, completion,
                     &SurfaceControlPresenter::onComplete);
    _f.transactionApply(transaction);
    _f.transactionDelete(transaction);
    _shared->lastToken = token;
    _shared->hasLastToken = true;
  }

private:
  struct Shared {
    std::mutex mutex;
    bool closed = false;
    SurfaceControlFunctions::Control *control = nullptr;
    ReleaseCallback onRelease;
    int viewWidth = 0;
    int viewHeight = 0;
    bool opaque = true;
    // The buffer currently posted, released when the next one is latched.
    uint64_t lastToken = 0;
    bool hasLastToken = false;
  };

  struct Completion {
    std::shared_ptr<Shared> shared;
    uint64_t previousToken;
    bool hasPrevious;
  };

  SurfaceControlPresenter(const SurfaceControlFunctions &f,
                          ANativeWindow *window,
                          SurfaceControlFunctions::Control *control)
      : _f(f), _window(window), _shared(std::make_shared<Shared>()) {
    _shared->control = control;
  }

  // Binder thread. The previous buffer of the surface is released now:
  // ASurfaceTransactionStats_getPreviousReleaseFenceFd returns the fence that
  // signals once the compositor is done reading it, or -1 when it already is.
  // The callback owns the descriptor.
  static void onComplete(void *context, SurfaceControlFunctions::Stats *stats) {
    std::unique_ptr<Completion> completion(static_cast<Completion *>(context));
    int releaseFenceFd = -1;
    ReleaseCallback onRelease;
    {
      std::lock_guard<std::mutex> lock(completion->shared->mutex);
      if (!completion->shared->closed &&
          completion->shared->control != nullptr) {
        releaseFenceFd =
            SurfaceControlFunctions::get().getPreviousReleaseFenceFd(
                stats, completion->shared->control);
        onRelease = completion->shared->onRelease;
      }
    }
    if (completion->hasPrevious && onRelease) {
      onRelease(completion->previousToken, releaseFenceFd); // owns the fd
    } else if (releaseFenceFd >= 0) {
      ::close(releaseFenceFd);
    }
  }

  const SurfaceControlFunctions &_f;
  ANativeWindow *_window;
  std::shared_ptr<Shared> _shared;
};

} // namespace rnwgpu
