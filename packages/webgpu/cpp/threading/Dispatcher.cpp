//
// Created by Marc Rousavy on 12.03.24.
//
#include <memory>
#include <unordered_map>

#include "Dispatcher.h"

#include "RNFJSIHelper.h"

namespace margelo {

namespace jsi = facebook::jsi;

static constexpr auto GLOBAL_DISPATCHER_HOLDER_NAME = "__nitroDispatcher";

std::unordered_map<jsi::Runtime *, std::weak_ptr<Dispatcher>>
    Dispatcher::_globalCache;

void Dispatcher::installRuntimeGlobalDispatcher(
    jsi::Runtime &runtime, std::shared_ptr<Dispatcher> dispatcher) {

  // Store a weak reference in global cache
  _globalCache[&runtime] = std::weak_ptr<Dispatcher>(dispatcher);

  // Inject the dispatcher into Runtime global (runtime will hold a strong
  // reference)
  jsi::Object dispatcherHolder(runtime);
  dispatcherHolder.setNativeState(runtime, dispatcher);
  runtime.global().setProperty(runtime, GLOBAL_DISPATCHER_HOLDER_NAME,
                               dispatcherHolder);
}

std::shared_ptr<Dispatcher>
Dispatcher::getRuntimeGlobalDispatcher(jsi::Runtime &runtime) {
  if (auto search = _globalCache.find(&runtime); search != _globalCache.end()) {
    // the runtime is known - we have something in cache
    std::weak_ptr<Dispatcher> weakDispatcher = _globalCache[&runtime];
    std::shared_ptr<Dispatcher> strongDispatcher = weakDispatcher.lock();
    if (strongDispatcher) {
      // the weak reference we cached is still valid - return it!
      return strongDispatcher;
    }
  }

  jsi::Value dispatcherHolderValue = getRuntimeGlobalDispatcherHolder(runtime);
  jsi::Object dispatcherHolder = dispatcherHolderValue.getObject(runtime);
  return dispatcherHolder.getNativeState<Dispatcher>(runtime);
}

jsi::Value Dispatcher::getRuntimeGlobalDispatcherHolder(jsi::Runtime &runtime) {
  return runtime.global().getProperty(runtime, GLOBAL_DISPATCHER_HOLDER_NAME);
}

} // namespace margelo
