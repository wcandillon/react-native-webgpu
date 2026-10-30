#pragma once

#include <jsi/jsi.h>
#include <utility>
#include <vector>
#include <string>
#include <memory>

namespace rnwgpu {

namespace jsi = facebook::jsi;

class Promise {
public:
  Promise(jsi::Runtime& runtime, jsi::Function&& resolver, jsi::Function&& rejecter);

  void resolve(jsi::Value&& result);
  /** Rejects with a new Error carrying `error` as its message. */
  void reject(std::string error);
  /** Rejects with the JS error `error` carries, as thrown. */
  void reject(const jsi::JSError& error);

public:
  jsi::Runtime& runtime;

private:
  jsi::Function _resolver;
  jsi::Function _rejecter;

public:
  using RunPromise = std::function<void(jsi::Runtime& runtime, std::shared_ptr<Promise> promise)>;
  /**
   Create a new Promise and runs the given `run` function.
   */
  static jsi::Value createPromise(jsi::Runtime& runtime, RunPromise run);
};

} // namespace rnwgpu
