#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <atomic>
#include <memory>
#include <utility>

#include "NativeBufferViewRegistry.h"

namespace rnwgpu {

namespace {

// A fence that has not signaled after this long belongs to work that will
// never finish (a lost device): the buffer is dropped, as on Android.
constexpr int64_t kTimeoutSeconds = 10;

// One wait, finished by whichever comes first: the event's notification or
// the timeout. Metal cannot cancel a notification, so after a timeout the
// listener keeps its block (and this small object) until the event signals,
// possibly never; the fence and the buffer captured by `done` are released
// when the wait finishes. ANGLE's Metal sync (SyncMtl.mm) makes the same
// trade-off.
struct PendingFenceWait {
  wgpu::SharedFence fence;
  std::function<void(bool)> done;
  std::atomic<bool> finished{false};

  void finish(bool signaled) {
    if (finished.exchange(true)) {
      return;
    }
    auto callback = std::move(done);
    fence = nullptr;
    if (!signaled) {
      NSLog(@"[WebGPU] presentNativeBuffer: the fence did not signal within "
            @"%lld s; the buffer is dropped",
            kTimeoutSeconds);
    }
    callback(signaled);
  }
};

} // namespace

// The MTLSharedEvent behind a Dawn fence, waited on with a listener: no
// thread blocks. The notification and the timeout run on a private serial
// queue.
void waitForSharedFence(wgpu::SharedFence fence, uint64_t signaledValue,
                        std::function<void(bool)> done) {
  wgpu::SharedFenceExportInfo info{};
  wgpu::SharedFenceMTLSharedEventExportInfo mtlInfo{};
  info.nextInChain = &mtlInfo;
  fence.ExportInfo(&info);
  id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)mtlInfo.sharedEvent;
  if (info.type != wgpu::SharedFenceType::MTLSharedEvent || event == nil) {
    NSLog(@"[WebGPU] presentNativeBuffer: the fence is not an MTLSharedEvent; "
          @"the buffer is dropped");
    done(false);
    return;
  }
  if (event.signaledValue >= signaledValue) {
    done(true);
    return;
  }
  static dispatch_queue_t queue = dispatch_queue_create(
      "com.webgpu.NativeBufferView.fence", DISPATCH_QUEUE_SERIAL);
  static MTLSharedEventListener *listener =
      [[MTLSharedEventListener alloc] initWithDispatchQueue:queue];
  // Holds the fence (which keeps the event, and Dawn's view of it, alive)
  // until the wait finishes.
  auto pending = std::make_shared<PendingFenceWait>();
  pending->fence = std::move(fence);
  pending->done = std::move(done);
  [event
      notifyListener:listener
             atValue:signaledValue
               block:^(id<MTLSharedEvent> /* event */, uint64_t /* value */) {
                 pending->finish(true);
               }];
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kTimeoutSeconds * NSEC_PER_SEC), queue,
      ^{
        pending->finish(false);
      });
}

} // namespace rnwgpu
