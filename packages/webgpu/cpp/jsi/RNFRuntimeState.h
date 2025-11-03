#pragma once

#include <jsi/jsi.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace rnwgpu {

namespace jsi = facebook::jsi;

/**
 * Runtime state management using Hermes' runtime.setRuntimeData API.
 * This replaces the old RuntimeLifecycleMonitor/RuntimeAwareCache system.
 */
class RNFRuntimeState {
public:
  // UUID key for storing our runtime state
  static const jsi::UUID kRuntimeStateKey;

  /**
   * Get or create the runtime state for the given runtime
   */
  static std::shared_ptr<RNFRuntimeState> get(jsi::Runtime& runtime);

  /**
   * Template cache that can store any type T per object pointer
   */
  template <typename T>
  class ObjectCache {
  public:
    std::shared_ptr<T> getOrCreate(void* object) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = cache_.find(object);
      if (it != cache_.end()) {
        return it->second;
      }

      auto value = std::make_shared<T>();
      cache_[object] = value;
      return value;
    }

    void remove(void* object) {
      std::lock_guard<std::mutex> lock(mutex_);
      cache_.erase(object);
    }

    void clear() {
      std::lock_guard<std::mutex> lock(mutex_);
      cache_.clear();
    }

  private:
    std::mutex mutex_;
    std::unordered_map<void*, std::shared_ptr<T>> cache_;
  };

  /**
   * Get or create a cache for a specific type T
   */
  template <typename T>
  std::shared_ptr<ObjectCache<T>> getCache() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Use type_info as key for the cache type
    const std::type_info& typeId = typeid(T);
    auto it = typeCaches_.find(&typeId);

    if (it != typeCaches_.end()) {
      return std::static_pointer_cast<ObjectCache<T>>(it->second);
    }

    auto cache = std::make_shared<ObjectCache<T>>();
    typeCaches_[&typeId] = cache;
    return cache;
  }

private:
  RNFRuntimeState() = default;

  std::mutex mutex_;
  // Map from type_info to cache instance
  std::unordered_map<const std::type_info*, std::shared_ptr<void>> typeCaches_;
};

/**
 * Template helper for runtime-aware caching compatible with the old API
 * This provides a migration path from RuntimeAwareCache
 */
template <typename T>
class RuntimeAwareCache {
public:
  T& get(jsi::Runtime& rt) {
    auto state = RNFRuntimeState::get(rt);
    auto cache = state->getCache<T>();

    // For compatibility, we use the runtime pointer as the object key
    auto ptr = cache->getOrCreate(&rt);
    return *ptr;
  }
};

} // namespace rnwgpu
