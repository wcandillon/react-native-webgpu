#pragma once

#include <exception>
#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace rnwgpu {

namespace jsi = facebook::jsi;

class Promise;

/**
 * Owns the JSI-backed state for Promises created in one runtime.
 *
 * The context itself is stored in that runtime's runtimeData. Its destructor
 * therefore runs while the runtime can still safely release jsi::Function
 * values. Native callbacks may keep Promise objects alive for longer, but
 * invalidate() removes every runtime-backed value before the runtime goes away.
 */
class PromiseRuntimeContext final
    : public std::enable_shared_from_this<PromiseRuntimeContext> {
public:
  explicit PromiseRuntimeContext(jsi::Runtime &runtime) noexcept;
  ~PromiseRuntimeContext();

  PromiseRuntimeContext(const PromiseRuntimeContext &) = delete;
  PromiseRuntimeContext &operator=(const PromiseRuntimeContext &) = delete;
  PromiseRuntimeContext(PromiseRuntimeContext &&) = delete;
  PromiseRuntimeContext &operator=(PromiseRuntimeContext &&) = delete;

  static std::shared_ptr<PromiseRuntimeContext>
  getOrCreate(jsi::Runtime &runtime);

  void invalidate() noexcept;
  bool isValid() const noexcept;
  const void *runtimeIdentity() const noexcept;

  /**
   * Run an action only while this context still owns a live runtime.
   * Callers must already be executing on that runtime's thread.
   */
  bool withRuntime(const std::function<void(jsi::Runtime &)> &action);

  void trackPromise(const std::shared_ptr<Promise> &promise);
  void untrackPromise(const Promise *promise) noexcept;

private:
  static jsi::UUID runtimeDataUUID();

  mutable std::recursive_mutex _mutex;
  jsi::Runtime *_runtime;
  std::unordered_map<const Promise *, std::shared_ptr<Promise>> _promises;
};

class Promise final : public std::enable_shared_from_this<Promise> {
public:
  using RunPromise = std::function<void(jsi::Runtime &runtime,
                                        std::shared_ptr<Promise> promise)>;

  Promise(std::weak_ptr<PromiseRuntimeContext> context,
          jsi::Function &&resolver, jsi::Function &&rejecter);

  Promise(const Promise &) = delete;
  Promise &operator=(const Promise &) = delete;
  Promise(Promise &&) = delete;
  Promise &operator=(Promise &&) = delete;

  void resolve(jsi::Value &&result);
  void reject(std::string error);

  /** Release resolver/rejecter while their runtime is still alive. */
  void invalidate() noexcept;

  bool withRuntime(const std::function<void(jsi::Runtime &)> &action) const;
  bool belongsToRuntime(const void *runtimeIdentity) const noexcept;

  template <typename ValueFactory>
  void resolveWith(ValueFactory &&valueFactory) {
    auto factory = std::forward<ValueFactory>(valueFactory);
    withRuntime(
        [this, factory = std::move(factory)](jsi::Runtime &runtime) mutable {
          try {
            resolve(jsi::Value(factory(runtime)));
          } catch (const std::exception &exception) {
            reject(exception.what());
          } catch (...) {
            reject("Unknown native error while resolving Promise");
          }
        });
  }

  /** Create a Promise owned by the calling runtime's runtimeData. */
  static jsi::Value createPromise(jsi::Runtime &runtime, RunPromise run);

private:
  std::weak_ptr<PromiseRuntimeContext> _context;
  mutable std::mutex _mutex;
  std::optional<jsi::Function> _resolver;
  std::optional<jsi::Function> _rejecter;
  bool _settled{false};
};

} // namespace rnwgpu
