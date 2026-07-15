#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace rnwgpu::async {

// Address-keyed cache of per-runtime singletons. Used instead of
// jsi::Runtime::set/getRuntimeData: Hermes backs runtimeData with a DenseMap
// whose reserved empty key is the default-constructed (all-zero) jsi::UUID,
// so caching under jsi::UUID() reads uninitialized buckets and crashes on any
// runtime that already carries other runtime data (e.g. react-native-worklets
// runtimes). The host keeps each jsi::Runtime object stable for its lifetime,
// so its address is a safe identity; expired entries are pruned lazily.
template <typename Key, typename Value> class RuntimeRegistry {
public:
  std::shared_ptr<Value> get(Key *key) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(key);
    if (it == _entries.end()) {
      return nullptr;
    }
    auto value = it->second.lock();
    if (!value) {
      _entries.erase(it);
    }
    return value;
  }

  template <typename Factory>
  std::shared_ptr<Value> getOrCreate(Key *key, Factory &&factory) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto &slot = _entries[key];
    if (auto value = slot.lock()) {
      return value;
    }
    std::shared_ptr<Value> value = std::forward<Factory>(factory)();
    slot = value;
    return value;
  }

private:
  std::mutex _mutex;
  std::unordered_map<Key *, std::weak_ptr<Value>> _entries;
};

} // namespace rnwgpu::async
