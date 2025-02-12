#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>

#include "CallInvokerDispatcher.h"

namespace rnwgpu {

// Global registry to track dispatchers per runtime
class RuntimeDispatcherRegistry {
private:
    std::mutex _mutex;
    std::unordered_map<jsi::Runtime*, std::shared_ptr<CallInvokerDispatcher>> _dispatchers;
    
    RuntimeDispatcherRegistry() = default;

public:
    static RuntimeDispatcherRegistry& getInstance() {
        static RuntimeDispatcherRegistry instance;
        return instance;
    }

    void registerDispatcher(jsi::Runtime* runtime, std::shared_ptr<CallInvokerDispatcher> dispatcher) {
        std::lock_guard<std::mutex> lock(_mutex);
        _dispatchers[runtime] = dispatcher;
    }

    void unregisterDispatcher(jsi::Runtime* runtime) {
        std::lock_guard<std::mutex> lock(_mutex);
        _dispatchers.erase(runtime);
    }

    std::shared_ptr<CallInvokerDispatcher> getDispatcher(jsi::Runtime* runtime) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _dispatchers.find(runtime);
        if (it == _dispatchers.end()) {
            throw std::runtime_error("No dispatcher registered for this runtime");
        }
        return it->second;
    }
};

} // namespace rnwgpu
