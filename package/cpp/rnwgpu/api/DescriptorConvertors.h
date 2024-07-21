#pragma once

#include <memory>
#include <vector>

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

  static jsi::Value toJSI(jsi::Runtime &runtime, Target arg) {
    if (std::holds_alternative<std::nullptr_t>(arg)) {
      return jsi::Value::null();
    }
    return JSIConverter<std::shared_ptr<O>>::toJSI(
        runtime, std::get<std::shared_ptr<O>>(arg));
  }
};

// TODO: careful std::variant<O, std::nullptr_t> doesn't overload
// std::variant<std::nullptr_t, 0> (order's matter)
// variant<nullptr_t, numeric>
template <typename O>
struct JSIConverter<
    std::variant<O, std::nullptr_t>,
    std::enable_if_t<std::is_arithmetic_v<O> || std::is_enum_v<O>>> {
  using Target = std::variant<O, std::nullptr_t>;
  static Target fromJSI(jsi::Runtime &runtime, const jsi::Value &arg,
                        bool outOfBound) {
    if (arg.isNull()) {
      return Target(nullptr);
    }
    if (arg.isNumber()) {
      return Target(static_cast<O>(arg.asNumber()));
    }
    throw jsi::JSError(runtime, "Expected null or number");
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, Target arg) {
    if (std::holds_alternative<std::nullptr_t>(arg)) {
      return jsi::Value::null();
    }
    return jsi::Value(static_cast<double>(std::get<O>(arg)));
  }
};

} // namespace margelo
