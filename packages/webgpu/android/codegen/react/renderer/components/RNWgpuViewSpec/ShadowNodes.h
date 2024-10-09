#pragma once

#include <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#include <react/renderer/components/RNWgpuViewSpec/Props.h>
#include <react/renderer/components/RNWgpuViewSpec/States.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <jsi/jsi.h>
#include <react/renderer/core/LayoutContext.h>

#include "../../../../../StaticDataBridge/SizeHolder.h"

namespace facebook::react {

JSI_EXPORT extern const char WebGPUViewComponentName[];

/*
 * `ShadowNode` for <WebGPUView> component.
 */
using WebGPUViewShadowNode = ConcreteViewShadowNode<
    WebGPUViewComponentName,
    WebGPUViewProps,
    WebGPUViewEventEmitter,
    WebGPUViewState>;

class WebGPUViewCustomShadowNode final : public WebGPUViewShadowNode {

public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  void layout(LayoutContext layoutContext) override {
    YogaLayoutableShadowNode::layout(layoutContext);
    configureSurface();
  }

  void configureSurface() {
    const auto &viewProps = *std::static_pointer_cast<WebGPUViewProps const>(props_);
    rnwgpu::SizeHolder::setSize(
      viewProps.contextId,
      { layoutMetrics_.frame.size.width, layoutMetrics_.frame.size.height }
    );
  }

};

} // namespace facebook::react
