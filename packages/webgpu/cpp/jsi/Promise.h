#pragma once

#include <jsi/jsi.h>
#include <utility>
#include <vector>
#include <string>
#include <memory>

namespace rnwgpu {

namespace jsi = facebook::jsi;

/**
 * A context that wraps a runtime pointer and can be invalidated
 * when the runtime is torn down (e.g., during hot reload).
 */
class RuntimeContext : public std::enable_shared_from_this<RuntimeContext> {
public:
  explicit RuntimeContext(jsi::Runtime& runtime) : _runtime(&runtime) {}

  void invalidate() { _runtime = nullptr; }

  jsi::Runtime* getRuntime() const { return _runtime; }

  bool isValid() const { return _runtime != nullptr; }

  /**
   * Set the main runtime context (called during module initialization).
   */
  static void setMainContext(std::shared_ptr<RuntimeContext> context) {
    _mainContext = std::move(context);
  }

  /**
   * Get the main runtime context (may be nullptr if not set or invalidated).
   */
  static std::shared_ptr<RuntimeContext> getMainContext() {
    return _mainContext;
  }

private:
  jsi::Runtime* _runtime;
  static std::shared_ptr<RuntimeContext> _mainContext;
};

class Promise {
public:
  Promise(std::weak_ptr<RuntimeContext> context, jsi::Function&& resolver,
          jsi::Function&& rejecter);

  void resolve(jsi::Value&& result);
  void reject(std::string error);

  /**
   * Get the runtime pointer, or nullptr if the runtime has been torn down.
   * Use this to safely construct jsi::Value before calling resolve().
   */
  jsi::Runtime* getRuntime() const;

  /**
   * Resolve with a value constructed by the factory function.
   * The factory is only called if the runtime is still valid.
   * Usage: promise->resolveWith([&](jsi::Runtime& rt) { return
   * JSIConverter<T>::toJSI(rt, value); });
   */
  template <typename F> void resolveWith(F&& valueFactory) {
    auto context = _context.lock();
    if (!context || !context->isValid()) {
      return;
    }
    auto* runtime = context->getRuntime();
    _resolver.call(*runtime, valueFactory(*runtime));
  }

private:
  std::weak_ptr<RuntimeContext> _context;
  jsi::Function _resolver;
  jsi::Function _rejecter;

public:
  using RunPromise =
      std::function<void(jsi::Runtime& runtime, std::shared_ptr<Promise> promise)>;
  /**
   Create a new Promise and runs the given `run` function.
   */
  static jsi::Value createPromise(std::shared_ptr<RuntimeContext> context,
                                  RunPromise run);
};

} // namespace rnwgpu
