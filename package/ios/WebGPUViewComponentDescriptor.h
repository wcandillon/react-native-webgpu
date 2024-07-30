#ifdef RCT_NEW_ARCH_ENABLED

#pragma once

#include <react/debug/react_native_assert.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/core/LayoutContext.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/components/RNWgpuViewSpec/ShadowNodes.h>

#import "MetalView.h"
#import "WebGPUView.h"

namespace facebook {
namespace react {

class WebGPUViewCustomShadowNode final : public WebGPUViewShadowNode {

bool isConfigured_ = false;

public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;
 
  void layout(LayoutContext layoutContext) override {
    YogaLayoutableShadowNode::layout(layoutContext);
    if (!isConfigured_) {
      const auto &viewProps = *std::static_pointer_cast<WebGPUViewProps const>(props_);
      wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
      
      __block MetalView *metalView;
      __block CALayer *layer;
      dispatch_sync(dispatch_get_main_queue(), ^{
        metalView = [[MetalView alloc] init];
        layer = metalView.layer;
      });
      [WebGPUView registerMetalView:metalView withContextId:@(viewProps.contextId)];
    
      metalSurfaceDesc.layer = (void *)CFBridgingRetain(layer);
      wgpu::SurfaceDescriptor surfaceDescriptor;
      surfaceDescriptor.nextInChain = &metalSurfaceDesc;
      std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
      auto surfaceGpu = std::make_shared<wgpu::Surface>(manager->getGPU()->get().CreateSurface(&surfaceDescriptor));
      float width = layoutMetrics_.frame.size.width;
      float height = layoutMetrics_.frame.size.height;
      rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
      manager->surfacesRegistry.addSurface(viewProps.contextId, surfaceData);
    }
  }
};

class WebGPUViewComponentDescriptor final : public ConcreteComponentDescriptor<WebGPUViewCustomShadowNode> {
public:
using ConcreteComponentDescriptor::ConcreteComponentDescriptor;
};

} // namespace react
} // namespace facebook

#endif // RCT_NEW_ARCH_ENABLED
