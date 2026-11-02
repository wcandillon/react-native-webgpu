#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <jsi/jsi.h>

namespace rnwgpu {
class Promise;
}

namespace rnwgpu::async {

class RuntimeContext;

/**
 * Represents a pending asynchronous WebGPU operation that can be converted into
 * a JavaScript Promise.
 *
 * The resolve and reject functions run on any thread (the instance's waiter
 * thread, usually) and never touch JSI: they hand the settling to the owning
 * RuntimeContext, which runs it on its runtime's own thread (see
 * RuntimeContext::post).
 */
class AsyncTaskHandle {
public:
  struct State;

  using ValueFactory =
      std::function<facebook::jsi::Value(facebook::jsi::Runtime &)>;
  using ResolveFunction = std::function<void(ValueFactory)>;
  using RejectFunction = std::function<void(std::string)>;

  AsyncTaskHandle();

  /**
   * Internal constructor used by RuntimeContext.
   */
  explicit AsyncTaskHandle(std::shared_ptr<State> state);

  bool valid() const;

  ResolveFunction createResolveFunction() const;
  RejectFunction createRejectFunction() const;

  void attachPromise(const std::shared_ptr<rnwgpu::Promise> &promise) const;

  static AsyncTaskHandle create(const std::shared_ptr<RuntimeContext> &context);

private:
  std::shared_ptr<State> _state;
};

} // namespace rnwgpu::async
