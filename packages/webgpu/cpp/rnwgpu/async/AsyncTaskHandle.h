#pragma once

#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace rnwgpu {
class Promise;
}

namespace rnwgpu::async {

class RuntimeContext;

/**
 * Native state for one asynchronous WebGPU operation.
 *
 * RuntimeContext owns pending states. A State references its context weakly,
 * so there is no ownership cycle; runtimeData teardown cancels every state and
 * releases its Promise's JSI functions before the runtime disappears.
 */
class AsyncTaskHandle final {
public:
  struct State;

  using ValueFactory =
      std::function<facebook::jsi::Value(facebook::jsi::Runtime &)>;
  using ResolveFunction = std::function<void(ValueFactory)>;
  using RejectFunction = std::function<void(std::string)>;

  AsyncTaskHandle() = default;
  explicit AsyncTaskHandle(std::shared_ptr<State> state);

  bool valid() const noexcept;
  ResolveFunction createResolveFunction() const;
  RejectFunction createRejectFunction() const;
  void attachPromise(const std::shared_ptr<rnwgpu::Promise> &promise) const;
  void cancel() const noexcept;

  static AsyncTaskHandle create(const std::shared_ptr<RuntimeContext> &context,
                                bool keepPumping);

private:
  std::shared_ptr<State> _state;
};

} // namespace rnwgpu::async
