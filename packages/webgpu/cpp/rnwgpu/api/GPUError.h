#pragma once

#include <string>
#include <utility>

#include "NativeObject.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUError : public NativeObject<GPUError> {
public:
  static constexpr const char *CLASS_NAME = "GPUError";

  explicit GPUError(std::string message)
      : NativeObject(CLASS_NAME), _message(std::move(message)) {}

public:
  std::string getBrand() { return CLASS_NAME; }
  std::string getMessage() { return _message; }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUError::getBrand);
    installGetter(runtime, prototype, "message", &GPUError::getMessage);
  }

  // GPUValidationError, GPUOutOfMemoryError and GPUInternalError extend
  // GPUError in the spec: chain their prototype to GPUError.prototype so
  // `error instanceof GPUError` holds.
  static void inheritPrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installPrototype(runtime);
    auto *parent = getCachedPrototype(runtime);
    if (parent == nullptr) {
      return;
    }
    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto setPrototypeOf =
        objectCtor.getPropertyAsFunction(runtime, "setPrototypeOf");
    setPrototypeOf.call(runtime, prototype, *parent);
  }

protected:
  // Protected constructor for subclasses
  GPUError(const char *className, std::string message)
      : NativeObject(className), _message(std::move(message)) {}

  std::string _message;
};

} // namespace rnwgpu
