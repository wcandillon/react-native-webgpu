//
// Created by Marc Rousavy on 22.02.24.
//
#pragma once

#include "RNFWorkletRuntimeState.h"
#include "WGPULogger.h"

#include <jsi/jsi.h>

#include <memory>

namespace margelo {

// From:
// https://github.com/software-mansion/react-native-reanimated/blob/6cb1a66f1a68cac8079de2b6b305d22359847e51/Common/cpp/ReanimatedRuntime/WorkletRuntimeCollector.h
class WorkletRuntimeCollector : public jsi::HostObject {
  // When worklet runtime is created, we inject an instance of this class as a
  // `jsi::HostObject` into the global object. When worklet runtime is
  // terminated, the object is garbage-collected, which runs the C++ destructor.
  // We only keep the state alive while the runtime exists; destruction is purely for logging.

public:
  explicit WorkletRuntimeCollector(jsi::Runtime& runtime) : _runtime(runtime) {
    Logger::log("WorkletRuntimeCollector", "Registering WorkletRuntime %p", &runtime);
    WorkletRuntimeState::get(_runtime);
  }

  ~WorkletRuntimeCollector() {
    Logger::log("WorkletRuntimeCollector", "Unregistering WorkletRuntime %p", &_runtime);
  }

  static void install(jsi::Runtime& rt) {
    auto collector = std::make_shared<WorkletRuntimeCollector>(rt);
    auto object = jsi::Object::createFromHostObject(rt, collector);
    rt.global().setProperty(rt, "__workletRuntimeCollector", object);
  }

private:
  jsi::Runtime& _runtime;
};

} // namespace margelo
