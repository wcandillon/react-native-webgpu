#ifdef RCT_NEW_ARCH_ENABLED

#pragma once

#include <react/debug/react_native_assert.h>
#include <react/renderer/components/RNWgpuViewSpec/ShadowNodes.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/core/LayoutContext.h>

#import "MetalView.h"
#import "SurfaceUtils.h"
#import "WebGPUView.h"

namespace facebook {
namespace react {

class WebGPUViewCustomShadowNode final : public WebGPUViewShadowNode {

public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  void layout(LayoutContext layoutContext) override {
    YogaLayoutableShadowNode::layout(layoutContext);
    configureSurface();
  }

  void configureSurface() {
    const auto &viewProps =
        *std::static_pointer_cast<WebGPUViewProps const>(props_);
    if ([WebGPUView isContextRegisterd:@(viewProps.contextId)]) {
      return;
    }
    __block MetalView *metalView;
    __block CALayer *layer;
    dispatch_sync(dispatch_get_main_queue(), ^{
      metalView = [[MetalView alloc] init];
      layer = metalView.layer;
    });
    [WebGPUView registerMetalView:metalView
                    withContextId:@(viewProps.contextId)];

    // TODO: use physical size
    float width = layoutMetrics_.frame.size.width;
    float height = layoutMetrics_.frame.size.height;
    [SurfaceUtils configureSurface:layer
                              size:CGSizeMake(width, height)
                         contextId:viewProps.contextId];
  }
};

class WebGPUViewComponentDescriptor final
    : public ConcreteComponentDescriptor<WebGPUViewCustomShadowNode> {
public:
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;
};

} // namespace react
} // namespace facebook

#endif // RCT_NEW_ARCH_ENABLED
