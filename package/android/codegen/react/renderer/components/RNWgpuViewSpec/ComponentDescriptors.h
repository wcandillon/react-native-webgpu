#pragma once

#include <react/renderer/components/RNWgpuViewSpec/ShadowNodes.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/core/LayoutContext.h>

#include "../../../../../StaticDataBridge/SizeHolder.h"

namespace facebook::react {


class WebGPUViewCustomShadowNode final : public WebGPUViewShadowNode {

  bool isConfigured_ = false;

public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  void layout(LayoutContext layoutContext) override {
    YogaLayoutableShadowNode::layout(layoutContext);
    configureSurface();
  }

  void configureSurface() {
    if (isConfigured_) {
      return;
    }
    isConfigured_ = true;
    const auto &viewProps = *std::static_pointer_cast<WebGPUViewProps const>(props_);
    rnwgpu::SizeHolder::setSize(
      viewProps.contextId,
      {
        layoutMetrics_.frame.size.width,
        layoutMetrics_.frame.size.height
      }
    );
  }

};

class WebGPUViewComponentDescriptor final
    : public ConcreteComponentDescriptor<WebGPUViewCustomShadowNode> {
  public:
    using ConcreteComponentDescriptor::ConcreteComponentDescriptor;
};

void RNWgpuViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

} // namespace facebook::react
