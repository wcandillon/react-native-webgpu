#include "Promise.h"
#include <future>
#include <jsi/jsi.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rnwgpu {

namespace jsi = facebook::jsi;

// Static member definition
std::shared_ptr<RuntimeContext> RuntimeContext::_mainContext = nullptr;

Promise::Promise(std::weak_ptr<RuntimeContext> context, jsi::Function&& resolver,
                 jsi::Function&& rejecter)
    : _context(std::move(context)), _resolver(std::move(resolver)),
      _rejecter(std::move(rejecter)) {}

jsi::Value Promise::createPromise(std::shared_ptr<RuntimeContext> context,
                                  RunPromise run) {
  auto runtime = context->getRuntime();
  // Get Promise ctor from global
  auto promiseCtor = runtime->global().getPropertyAsFunction(*runtime, "Promise");

  auto promiseCallback = jsi::Function::createFromHostFunction(
      *runtime, jsi::PropNameID::forUtf8(*runtime, "PromiseCallback"), 2,
      [context, run](jsi::Runtime& runtime, const jsi::Value& thisValue,
                     const jsi::Value* arguments,
                     size_t count) -> jsi::Value {
        // Call function
        auto resolver = arguments[0].asObject(runtime).asFunction(runtime);
        auto rejecter = arguments[1].asObject(runtime).asFunction(runtime);
        auto promise = std::make_shared<Promise>(context, std::move(resolver),
                                                 std::move(rejecter));
        run(runtime, promise);

        return jsi::Value::undefined();
      });

  return promiseCtor.callAsConstructor(*runtime, promiseCallback);
}

void Promise::resolve(jsi::Value&& result) {
  auto context = _context.lock();
  if (!context || !context->isValid()) {
    // Runtime has been torn down, silently ignore
    return;
  }
  auto runtime = context->getRuntime();
  _resolver.call(*runtime, std::move(result));
}

void Promise::reject(std::string message) {
  auto context = _context.lock();
  if (!context || !context->isValid()) {
    // Runtime has been torn down, silently ignore
    return;
  }
  auto runtime = context->getRuntime();
  jsi::JSError error(*runtime, message);
  _rejecter.call(*runtime, error.value());
}

jsi::Runtime* Promise::getRuntime() const {
  auto context = _context.lock();
  if (!context || !context->isValid()) {
    return nullptr;
  }
  return context->getRuntime();
}

} // namespace rnwgpu
