#pragma once

#include <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#include <react/renderer/components/RNWgpuViewSpec/Props.h>
#include <react/renderer/components/RNWgpuViewSpec/States.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <jsi/jsi.h>

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

} // namespace facebook::react
