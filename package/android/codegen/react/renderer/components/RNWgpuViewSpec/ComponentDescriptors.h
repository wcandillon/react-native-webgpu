#pragma once

#include <react/renderer/components/RNWgpuViewSpec/ShadowNodes.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

extern const char WebGPUViewComponentName[] = "WebGPUView";

class WebGPUViewComponentDescriptor final
    : public ConcreteComponentDescriptor<WebGPUViewCustomShadowNode> {
  public:
    using ConcreteComponentDescriptor::ConcreteComponentDescriptor;
};

void RNWgpuViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

} // namespace facebook::react
