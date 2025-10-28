//
// Created by Marc Rousavy on 22.02.24.
//

#include "RNFWorkletRuntimeState.h"

namespace margelo {

const facebook::jsi::UUID WorkletRuntimeState::kRuntimeStateKey = facebook::jsi::UUID();

std::shared_ptr<WorkletRuntimeState> WorkletRuntimeState::get(facebook::jsi::Runtime& runtime) {
  auto existing = runtime.getRuntimeData(kRuntimeStateKey);
  if (existing) {
    return std::static_pointer_cast<WorkletRuntimeState>(existing);
  }

  auto state = std::shared_ptr<WorkletRuntimeState>(new WorkletRuntimeState());
  runtime.setRuntimeData(kRuntimeStateKey, state);
  return state;
}

std::shared_ptr<WorkletRuntimeState::FunctionCache> WorkletRuntimeState::getOrCreateCache(HybridObject* object) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iterator = caches_.find(object);
  if (iterator != caches_.end()) {
    return iterator->second;
  }

  auto cache = std::make_shared<FunctionCache>();
  caches_.emplace(object, cache);
  return cache;
}

void WorkletRuntimeState::removeCacheFor(HybridObject* object) {
  std::lock_guard<std::mutex> lock(mutex_);
  caches_.erase(object);
}

} // namespace margelo
