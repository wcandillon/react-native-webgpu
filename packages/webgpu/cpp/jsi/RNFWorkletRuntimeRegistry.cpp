#include "RNFWorkletRuntimeRegistry.h"

namespace margelo {

std::unordered_map<jsi::Runtime*, std::weak_ptr<RNFWorkletRuntimeRegistry::RuntimeSentinel>> RNFWorkletRuntimeRegistry::registry_{};
std::mutex RNFWorkletRuntimeRegistry::mutex_{};
const jsi::UUID RNFWorkletRuntimeRegistry::uuid_ = jsi::UUID();

RNFWorkletRuntimeRegistry::RuntimeSentinel::RuntimeSentinel(jsi::Runtime* runtime) : runtime(runtime) {}

RNFWorkletRuntimeRegistry::RuntimeSentinel::~RuntimeSentinel() {
  RNFWorkletRuntimeRegistry::removeRuntime(runtime);
}

void RNFWorkletRuntimeRegistry::removeRuntime(jsi::Runtime* runtime) {
  std::lock_guard<std::mutex> lock(mutex_);
  registry_.erase(runtime);
}

void RNFWorkletRuntimeRegistry::registerRuntime(jsi::Runtime& runtime) {
  auto existing = runtime.getRuntimeData(uuid_);
  if (existing) {
    auto sentinel = std::static_pointer_cast<RuntimeSentinel>(existing);
    std::lock_guard<std::mutex> lock(mutex_);
    registry_[&runtime] = sentinel;
    return;
  }

  auto sentinel = std::make_shared<RuntimeSentinel>(&runtime);
  runtime.setRuntimeData(uuid_, std::static_pointer_cast<void>(sentinel));

  std::lock_guard<std::mutex> lock(mutex_);
  registry_[&runtime] = sentinel;
}

void RNFWorkletRuntimeRegistry::unregisterRuntime(jsi::Runtime& runtime) {
  auto existing = runtime.getRuntimeData(uuid_);
  if (!existing) {
    removeRuntime(&runtime);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_.erase(&runtime);
  }

  runtime.setRuntimeData(uuid_, std::shared_ptr<void>());
}

bool RNFWorkletRuntimeRegistry::isRuntimeAlive(jsi::Runtime* runtime) {
  if (runtime == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto iterator = registry_.find(runtime);
  if (iterator == registry_.end()) {
    return false;
  }

  if (iterator->second.expired()) {
    registry_.erase(iterator);
    return false;
  }

  return true;
}

} // namespace margelo
