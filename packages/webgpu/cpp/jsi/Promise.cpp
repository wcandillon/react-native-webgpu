#include "Promise.h"

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace rnwgpu {

namespace {

struct PromiseRuntimeData final {
  explicit PromiseRuntimeData(std::shared_ptr<PromiseRuntimeContext> context)
      : context(std::move(context)) {}

  ~PromiseRuntimeData() {
    if (context) {
      context->invalidate();
    }
  }

  std::shared_ptr<PromiseRuntimeContext> context;
};

} // namespace

PromiseRuntimeContext::PromiseRuntimeContext(jsi::Runtime &runtime) noexcept
    : _runtime(&runtime) {}

PromiseRuntimeContext::~PromiseRuntimeContext() { invalidate(); }

std::shared_ptr<PromiseRuntimeContext>
PromiseRuntimeContext::getOrCreate(jsi::Runtime &runtime) {
  if (const auto data = runtime.getRuntimeData(runtimeDataUUID())) {
    const auto runtimeData = std::static_pointer_cast<PromiseRuntimeData>(data);
    if (runtimeData->context &&
        runtimeData->context->runtimeIdentity() == &runtime) {
      return runtimeData->context;
    }
  }

  auto context = std::make_shared<PromiseRuntimeContext>(runtime);
  runtime.setRuntimeData(runtimeDataUUID(),
                         std::make_shared<PromiseRuntimeData>(context));
  return context;
}

void PromiseRuntimeContext::invalidate() noexcept {
  std::unordered_map<const Promise *, std::shared_ptr<Promise>> promises;
  {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_runtime == nullptr) {
      return;
    }

    // Make the runtime unavailable atomically, then destroy JSI functions
    // without holding the context mutex. invalidate() runs on the owning
    // runtime thread via runtimeData teardown/replacement.
    promises.swap(_promises);
    _runtime = nullptr;
  }

  for (auto &[address, promise] : promises) {
    (void)address;
    if (promise) {
      promise->invalidate();
    }
  }
}

bool PromiseRuntimeContext::isValid() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  return _runtime != nullptr;
}

const void *PromiseRuntimeContext::runtimeIdentity() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_mutex);
  return _runtime;
}

bool PromiseRuntimeContext::withRuntime(
    const std::function<void(jsi::Runtime &)> &action) {
  if (!action) {
    return false;
  }

  jsi::Runtime *runtime = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_runtime == nullptr) {
      return false;
    }
    runtime = _runtime;
  }

  // withRuntime is called only from this runtime's owning thread. Do not call
  // Promise constructors/resolvers or user callbacks under _mutex (CP.22).
  action(*runtime);
  return true;
}

void PromiseRuntimeContext::trackPromise(
    const std::shared_ptr<Promise> &promise) {
  if (!promise) {
    return;
  }

  bool shouldInvalidate = false;
  {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_runtime == nullptr) {
      shouldInvalidate = true;
    } else {
      _promises.insert_or_assign(promise.get(), promise);
    }
  }
  if (shouldInvalidate) {
    promise->invalidate();
  }
}

void PromiseRuntimeContext::untrackPromise(const Promise *promise) noexcept {
  if (promise == nullptr) {
    return;
  }

  std::lock_guard<std::recursive_mutex> lock(_mutex);
  _promises.erase(promise);
}

jsi::UUID PromiseRuntimeContext::runtimeDataUUID() {
  // Hermes reserves the all-zero and all-0xff UUID values in its DenseMap.
  static constexpr jsi::UUID uuid(0x4CA6E8C7, 0x79CB, 0x4C28, 0xAF31,
                                  0x8D59B641A92EULL);
  return uuid;
}

Promise::Promise(std::weak_ptr<PromiseRuntimeContext> context,
                 jsi::Function &&resolver, jsi::Function &&rejecter)
    : _context(std::move(context)), _resolver(std::move(resolver)),
      _rejecter(std::move(rejecter)) {}

jsi::Value Promise::createPromise(jsi::Runtime &runtime, RunPromise run) {
  auto context = PromiseRuntimeContext::getOrCreate(runtime);
  if (!context || !context->isValid()) {
    throw std::runtime_error(
        "Cannot create a Promise after its runtime was invalidated");
  }

  std::optional<jsi::Value> result;
  const bool created = context->withRuntime([&](jsi::Runtime &owningRuntime) {
    auto promiseConstructor =
        owningRuntime.global().getPropertyAsFunction(owningRuntime, "Promise");
    auto executor = jsi::Function::createFromHostFunction(
        owningRuntime,
        jsi::PropNameID::forUtf8(owningRuntime, "PromiseCallback"), 2,
        [context, run = std::move(run)](
            jsi::Runtime &callbackRuntime, const jsi::Value & /*thisValue*/,
            const jsi::Value *arguments, size_t count) mutable -> jsi::Value {
          if (count < 2 || arguments == nullptr || !arguments[0].isObject() ||
              !arguments[0]
                   .getObject(callbackRuntime)
                   .isFunction(callbackRuntime) ||
              !arguments[1].isObject() ||
              !arguments[1]
                   .getObject(callbackRuntime)
                   .isFunction(callbackRuntime)) {
            throw jsi::JSError(
                callbackRuntime,
                "Promise executor did not receive resolve/reject functions");
          }

          auto resolver = arguments[0]
                              .getObject(callbackRuntime)
                              .getFunction(callbackRuntime);
          auto rejecter = arguments[1]
                              .getObject(callbackRuntime)
                              .getFunction(callbackRuntime);
          auto promise = std::make_shared<Promise>(context, std::move(resolver),
                                                   std::move(rejecter));
          context->trackPromise(promise);
          try {
            run(callbackRuntime, promise);
          } catch (const std::exception &exception) {
            try {
              promise->reject(exception.what());
            } catch (...) {
              // The JavaScript Promise constructor will reject a throwing
              // executor. Drop our resolver copies first so a failed native
              // rejection cannot leave this Promise tracked indefinitely.
              promise->invalidate();
              throw;
            }
          } catch (...) {
            try {
              promise->reject("Unknown native error while starting Promise");
            } catch (...) {
              promise->invalidate();
              throw;
            }
          }
          return jsi::Value::undefined();
        });

    result.emplace(
        promiseConstructor.callAsConstructor(owningRuntime, executor));
  });

  if (!created || !result.has_value()) {
    throw std::runtime_error(
        "Promise runtime was invalidated while creating a Promise");
  }
  return std::move(*result);
}

void Promise::resolve(jsi::Value &&result) {
  auto keepAlive = shared_from_this();
  auto context = _context.lock();
  if (!context) {
    return;
  }

  // jsi::Value is move-only, while withRuntime intentionally takes a
  // copyable std::function. The shared holder keeps the closure copyable and
  // still moves the value exactly once into the JavaScript resolver.
  auto resultHolder = std::make_shared<jsi::Value>(std::move(result));

  context->withRuntime(
      [this, keepAlive, context, resultHolder](jsi::Runtime &runtime) mutable {
        std::optional<jsi::Function> resolver;
        {
          std::lock_guard<std::mutex> lock(_mutex);
          if (_settled || !_resolver.has_value()) {
            return;
          }
          _settled = true;
          resolver.emplace(std::move(*_resolver));
          _resolver.reset();
          _rejecter.reset();
        }

        context->untrackPromise(this);
        resolver->call(runtime, std::move(*resultHolder));
      });
}

void Promise::reject(std::string message) {
  auto keepAlive = shared_from_this();
  auto context = _context.lock();
  if (!context) {
    return;
  }

  context->withRuntime([this, keepAlive, context, message = std::move(message)](
                           jsi::Runtime &runtime) mutable {
    std::optional<jsi::Function> rejecter;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (_settled || !_rejecter.has_value()) {
        return;
      }
      _settled = true;
      rejecter.emplace(std::move(*_rejecter));
      _resolver.reset();
      _rejecter.reset();
    }

    context->untrackPromise(this);
    jsi::JSError error(runtime, message);
    rejecter->call(runtime, error.value());
  });
}

void Promise::invalidate() noexcept {
  try {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _settled = true;
      _resolver.reset();
      _rejecter.reset();
    }
    if (const auto context = _context.lock()) {
      context->untrackPromise(this);
    }
  } catch (...) {
    // Never propagate an exception through a runtimeData destructor.
  }
}

bool Promise::withRuntime(
    const std::function<void(jsi::Runtime &)> &action) const {
  const auto context = _context.lock();
  return context && context->withRuntime(action);
}

bool Promise::belongsToRuntime(const void *runtimeIdentity) const noexcept {
  const auto context = _context.lock();
  return context && runtimeIdentity != nullptr &&
         context->runtimeIdentity() == runtimeIdentity;
}

} // namespace rnwgpu
