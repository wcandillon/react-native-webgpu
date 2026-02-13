#pragma once

#include <memory>
#include <string>
#include <variant>

#include "JSIConverter.h"
#include "NativeObject.h"

#include "GPUInternalError.h"
#include "GPUOutOfMemoryError.h"
#include "GPUValidationError.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

using GPUErrorVariant = std::variant<std::shared_ptr<GPUValidationError>,
                                     std::shared_ptr<GPUOutOfMemoryError>,
                                     std::shared_ptr<GPUInternalError>>;

class GPUUncapturedErrorEvent : public NativeObject<GPUUncapturedErrorEvent> {
public:
  static constexpr const char *CLASS_NAME = "GPUUncapturedErrorEvent";

  explicit GPUUncapturedErrorEvent(GPUErrorVariant error)
      : NativeObject(CLASS_NAME), _error(std::move(error)) {}

public:
  std::string getBrand() { return CLASS_NAME; }
  std::string getType() { return "uncapturederror"; }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand",
                  &GPUUncapturedErrorEvent::getBrand);
    installGetter(runtime, prototype, "type",
                  &GPUUncapturedErrorEvent::getType);

    // Custom getter for error that handles the variant conversion
    auto errorGetter = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "get_error"), 0,
        [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args,
           size_t count) -> jsi::Value {
          auto native = GPUUncapturedErrorEvent::fromValue(rt, thisVal);
          return std::visit(
              [&rt](auto &&err) -> jsi::Value {
                using T = std::decay_t<decltype(err)>;
                return JSIConverter<T>::toJSI(rt, err);
              },
              native->_error);
        });

    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto defineProperty =
        objectCtor.getPropertyAsFunction(runtime, "defineProperty");

    jsi::Object descriptor(runtime);
    descriptor.setProperty(runtime, "get", errorGetter);
    descriptor.setProperty(runtime, "enumerable", true);
    descriptor.setProperty(runtime, "configurable", true);

    defineProperty.call(runtime, prototype,
                        jsi::String::createFromUtf8(runtime, "error"),
                        descriptor);
  }

private:
  GPUErrorVariant _error;
};

} // namespace rnwgpu
