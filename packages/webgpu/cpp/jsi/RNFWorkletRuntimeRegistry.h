//
// Created by Marc Rousavy on 22.02.24.
//
#pragma once

#include <jsi/jsi.h>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace jsi = facebook::jsi;

namespace margelo {

class RNFWorkletRuntimeRegistry {
public:
  static void registerRuntime(jsi::Runtime& runtime);
  static void unregisterRuntime(jsi::Runtime& runtime);
  static bool isRuntimeAlive(jsi::Runtime* runtime);

private:
  struct RuntimeSentinel {
    explicit RuntimeSentinel(jsi::Runtime* runtime);
    ~RuntimeSentinel();

    jsi::Runtime* runtime;
  };

  static void removeRuntime(jsi::Runtime* runtime);

  static std::unordered_map<jsi::Runtime*, std::weak_ptr<RuntimeSentinel>> registry_;
  static std::mutex mutex_;
  static const jsi::UUID uuid_;

  RNFWorkletRuntimeRegistry() {} // private ctor

  friend class WorkletRuntimeCollector;
};

} // namespace margelo
