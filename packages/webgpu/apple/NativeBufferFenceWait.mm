#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <memory>
#include <utility>

#include "NativeBufferViewRegistry.h"

namespace rnwgpu {

// The MTLSharedEvent behind a Dawn fence, waited on with a listener: no
// thread blocks. The notification block runs on a private serial queue.
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
  static MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
      initWithDispatchQueue:dispatch_queue_create(
                                "com.webgpu.NativeBufferView.fence",
                                DISPATCH_QUEUE_SERIAL)];
  // Held by the block until the notification: the fence keeps the event (and
  // Dawn's view of it) alive for the wait.
  auto pending =
      std::make_shared<std::pair<wgpu::SharedFence, std::function<void(bool)>>>(
          std::move(fence), std::move(done));
  [event
      notifyListener:listener
             atValue:signaledValue
               block:^(id<MTLSharedEvent> /* event */, uint64_t /* value */) {
                 auto callback = std::move(pending->second);
                 pending->first = nullptr;
                 callback(true);
               }];
}

} // namespace rnwgpu
