#include <react/renderer/components/RNWgpuViewSpec/ComponentDescriptors.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>

namespace facebook::react {

void RNWgpuViewSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry) {
registry->add(concreteComponentDescriptorProvider<WebGPUViewComponentDescriptor>());
}

} // namespace facebook::react
