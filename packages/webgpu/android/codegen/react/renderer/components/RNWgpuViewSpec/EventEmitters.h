#pragma once

#include <react/renderer/components/view/ViewEventEmitter.h>

namespace facebook::react {
class WebGPUViewEventEmitter : public ViewEventEmitter {
public:
  using ViewEventEmitter::ViewEventEmitter;
};
} // namespace facebook::react
