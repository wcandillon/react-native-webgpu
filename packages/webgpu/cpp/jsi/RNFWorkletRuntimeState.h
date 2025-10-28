//
// Created by Marc Rousavy on 22.02.24.
//
#pragma once

#include <jsi/jsi.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>

namespace margelo {

class HybridObject;

class WorkletRuntimeState : public std::enable_shared_from_this<WorkletRuntimeState> {
public:
  struct FunctionCache {
    std::unordered_map<std::string, std::shared_ptr<facebook::jsi::Function>> functions;
  };

  static std::shared_ptr<WorkletRuntimeState> get(facebook::jsi::Runtime& runtime);

  std::shared_ptr<FunctionCache> getOrCreateCache(HybridObject* object);
  void removeCacheFor(HybridObject* object);

private:
  WorkletRuntimeState() = default;

  std::mutex mutex_;
  std::unordered_map<HybridObject*, std::shared_ptr<FunctionCache>> caches_;

  static const facebook::jsi::UUID kRuntimeStateKey;
};

} // namespace margelo
