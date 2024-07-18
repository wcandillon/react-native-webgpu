#pragma once

#include "RNFJSIConverter.h"

namespace margelo {

template <typename O, typename T>
struct JSIConverter<std::variant<std::vector<T>, std::shared_ptr<O>>> {
  using Target = std::variant<std::vector<T>, std::shared_ptr<O>>;

  static Target fromJSI(jsi::Runtime &runtime, const jsi::Value &arg,
                        bool outOfBound) {
    if (arg.isObject()) {
      auto object = arg.getObject(runtime);
      if (object.isArray(runtime)) {
        return Target(
            JSIConverter<std::vector<T>>::fromJSI(runtime, arg, outOfBound));
      }
      throw std::runtime_error("Invalid variant type expected array");
    }
    return Target(
        JSIConverter<std::shared_ptr<O>>::fromJSI(runtime, arg, outOfBound));
  }

  static jsi::Value toJSI(jsi::Runtime &, Target arg) {
    return jsi::Value::null();
  }
};

template <typename O>
struct JSIConverter<std::variant<std::nullptr_t, std::shared_ptr<O>>> {
  using Target = std::variant<std::nullptr_t, std::shared_ptr<O>>;

  static Target fromJSI(jsi::Runtime &runtime, const jsi::Value &arg,
                        bool outOfBound) {
    if (arg.isNull()) {
      return Target(nullptr);
    }
    return Target(
        JSIConverter<std::shared_ptr<O>>::fromJSI(runtime, arg, outOfBound));
  }

  static jsi::Value toJSI(jsi::Runtime &, Target arg) {
    return jsi::Value::null();
  }
};

} // namespace margelo
