//
// NativeObject base class for JSI NativeState pattern
//

#pragma once

#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "RNFJSIHelper.h"
#include "RNFRuntimeState.h"
#include "WGPULogger.h"

// Forward declare to avoid circular dependency
namespace margelo {
template <typename ArgType, typename SFINAE> struct JSIConverter;
} // namespace margelo

// Include the converter - must come after forward declaration
#include "RNFJSIConverter.h"

namespace margelo {

namespace jsi = facebook::jsi;

/**
 * Per-runtime cache for prototype objects.
 * Uses a simple static map keyed by runtime pointer.
 * Each NativeObject<Derived> type has its own static cache.
 */
template <typename Derived> class PrototypeCache {
public:
  static std::shared_ptr<jsi::Object> get(jsi::Runtime &runtime) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(&runtime);
    if (it != cache_.end()) {
      return it->second;
    }
    return nullptr;
  }

  static void set(jsi::Runtime &runtime,
                  std::shared_ptr<jsi::Object> prototype) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[&runtime] = std::move(prototype);
  }

private:
  static inline std::mutex mutex_;
  static inline std::unordered_map<jsi::Runtime *,
                                   std::shared_ptr<jsi::Object>> cache_;
};

/**
 * Base class for native objects using the NativeState pattern.
 *
 * Instead of using HostObject (which intercepts all property access),
 * this pattern:
 * 1. Stores native data via jsi::Object::setNativeState()
 * 2. Installs methods on a shared prototype object (once per runtime)
 * 3. Creates plain JS objects that use the prototype chain
 *
 * Usage:
 * ```cpp
 * class MyClass : public NativeObject<MyClass> {
 * public:
 *   static constexpr const char* CLASS_NAME = "MyClass";
 *
 *   MyClass(...) : NativeObject(CLASS_NAME), ... {}
 *
 *   std::string getValue() { return _value; }
 *
 *   static void definePrototype(jsi::Runtime& rt, jsi::Object& proto) {
 *     installGetter(rt, proto, "value", &MyClass::getValue);
 *   }
 *
 * private:
 *   std::string _value;
 * };
 * ```
 */
template <typename Derived>
class NativeObject : public jsi::NativeState,
                     public std::enable_shared_from_this<Derived> {
public:
  // Marker type for SFINAE detection in JSIConverter
  using IsNativeObject = std::true_type;

  /**
   * Ensure the prototype is installed for this runtime.
   * Called automatically by create(), but can be called manually.
   */
  static void installPrototype(jsi::Runtime &runtime) {
    auto existing = PrototypeCache<Derived>::get(runtime);
    if (existing) {
      return; // Already installed
    }

    // Create prototype object
    jsi::Object prototype(runtime);

    // Let derived class define its methods/properties
    Derived::definePrototype(runtime, prototype);

    // Cache the prototype
    auto sharedProto = std::make_shared<jsi::Object>(std::move(prototype));
    PrototypeCache<Derived>::set(runtime, sharedProto);
  }

  /**
   * Create a JS object with native state attached.
   */
  static jsi::Value create(jsi::Runtime &runtime,
                           std::shared_ptr<Derived> instance) {
    installPrototype(runtime);

    // Create a new object
    jsi::Object obj(runtime);

    // Attach native state
    obj.setNativeState(runtime, instance);

    // Set prototype
    auto proto = PrototypeCache<Derived>::get(runtime);
    if (proto) {
      // Use Object.setPrototypeOf to set the prototype
      auto objectCtor =
          runtime.global().getPropertyAsObject(runtime, "Object");
      auto setPrototypeOf =
          objectCtor.getPropertyAsFunction(runtime, "setPrototypeOf");
      setPrototypeOf.call(runtime, obj, *proto);
    }

    // Set memory pressure hint for GC
    auto pressure = instance->getMemoryPressure();
    if (pressure > 0) {
      obj.setExternalMemoryPressure(runtime, pressure);
    }

    // Store runtime reference in instance
    instance->_creationRuntime = &runtime;
    instance->_runtimeState = rnwgpu::RNFRuntimeState::get(runtime);

    return std::move(obj);
  }

  /**
   * Get the native state from a JS value.
   * Throws if the value doesn't have the expected native state.
   */
  static std::shared_ptr<Derived> fromValue(jsi::Runtime &runtime,
                                            const jsi::Value &value) {
    if (!value.isObject()) {
      throw jsi::JSError(runtime, std::string("Expected ") +
                                      Derived::CLASS_NAME +
                                      " but got non-object");
    }
    jsi::Object obj = value.getObject(runtime);
    if (!obj.hasNativeState<Derived>(runtime)) {
      throw jsi::JSError(runtime, std::string("Expected ") +
                                      Derived::CLASS_NAME +
                                      " but got different type");
    }
    return obj.getNativeState<Derived>(runtime);
  }

  /**
   * Memory pressure for GC hints. Override in derived classes.
   */
  virtual size_t getMemoryPressure() { return 1024; }

protected:
  explicit NativeObject(const char *name) : _name(name) {
#if DEBUG && RNF_ENABLE_LOGS
    Logger::logToConsole("NativeObject", "(MEMORY) Creating %s... ✅", _name);
#endif
  }

  virtual ~NativeObject() {
#if DEBUG && RNF_ENABLE_LOGS
    Logger::log("NativeObject", "(MEMORY) Deleting %s... ❌", _name);
#endif
  }

  const char *_name;
  jsi::Runtime *_creationRuntime = nullptr;
  std::weak_ptr<rnwgpu::RNFRuntimeState> _runtimeState;

  jsi::Runtime *getCreationRuntime() const {
    if (_runtimeState.expired()) {
      return nullptr;
    }
    return _creationRuntime;
  }

  bool isRuntimeAlive() const { return !_runtimeState.expired(); }

  // ============================================================
  // Helper methods for definePrototype() implementations
  // ============================================================

  /**
   * Install a method on the prototype.
   */
  template <typename ReturnType, typename... Args>
  static void installMethod(jsi::Runtime &runtime, jsi::Object &prototype,
                            const char *name,
                            ReturnType (Derived::*method)(Args...)) {
    auto func = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, name), sizeof...(Args),
        [method](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          auto native = Derived::fromValue(rt, thisVal);
          return callMethod(native.get(), method, rt, args,
                            std::index_sequence_for<Args...>{}, count);
        });
    prototype.setProperty(runtime, name, func);
  }

  /**
   * Install a getter on the prototype.
   */
  template <typename ReturnType>
  static void installGetter(jsi::Runtime &runtime, jsi::Object &prototype,
                            const char *name, ReturnType (Derived::*getter)()) {
    // Create a getter function
    auto getterFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, std::string("get_") + name),
        0,
        [getter](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          auto native = Derived::fromValue(rt, thisVal);
          if constexpr (std::is_same_v<ReturnType, void>) {
            (native.get()->*getter)();
            return jsi::Value::undefined();
          } else {
            ReturnType result = (native.get()->*getter)();
            return JSIConverter<std::decay_t<ReturnType>>::toJSI(
                rt, std::move(result));
          }
        });

    // Use Object.defineProperty to create a proper getter
    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto defineProperty =
        objectCtor.getPropertyAsFunction(runtime, "defineProperty");

    jsi::Object descriptor(runtime);
    descriptor.setProperty(runtime, "get", getterFunc);
    descriptor.setProperty(runtime, "enumerable", true);
    descriptor.setProperty(runtime, "configurable", true);

    defineProperty.call(runtime, prototype,
                        jsi::String::createFromUtf8(runtime, name), descriptor);
  }

  /**
   * Install a setter on the prototype.
   */
  template <typename ValueType>
  static void installSetter(jsi::Runtime &runtime, jsi::Object &prototype,
                            const char *name,
                            void (Derived::*setter)(ValueType)) {
    auto setterFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, std::string("set_") + name),
        1,
        [setter](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          auto native = Derived::fromValue(rt, thisVal);
          auto value =
              JSIConverter<std::decay_t<ValueType>>::fromJSI(rt, args[0], false);
          (native.get()->*setter)(std::move(value));
          return jsi::Value::undefined();
        });

    // Use Object.defineProperty to create a proper setter
    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto defineProperty =
        objectCtor.getPropertyAsFunction(runtime, "defineProperty");

    // Check if property already has a getter
    auto getOwnPropertyDescriptor =
        objectCtor.getPropertyAsFunction(runtime, "getOwnPropertyDescriptor");
    auto existingDesc = getOwnPropertyDescriptor.call(
        runtime, prototype, jsi::String::createFromUtf8(runtime, name));

    jsi::Object descriptor(runtime);
    if (existingDesc.isObject()) {
      auto existingDescObj = existingDesc.getObject(runtime);
      if (existingDescObj.hasProperty(runtime, "get")) {
        descriptor.setProperty(
            runtime, "get", existingDescObj.getProperty(runtime, "get"));
      }
    }
    descriptor.setProperty(runtime, "set", setterFunc);
    descriptor.setProperty(runtime, "enumerable", true);
    descriptor.setProperty(runtime, "configurable", true);

    defineProperty.call(runtime, prototype,
                        jsi::String::createFromUtf8(runtime, name), descriptor);
  }

  /**
   * Install both getter and setter for a property.
   */
  template <typename ReturnType, typename ValueType>
  static void installGetterSetter(jsi::Runtime &runtime, jsi::Object &prototype,
                                  const char *name,
                                  ReturnType (Derived::*getter)(),
                                  void (Derived::*setter)(ValueType)) {
    auto getterFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, std::string("get_") + name),
        0,
        [getter](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          auto native = Derived::fromValue(rt, thisVal);
          ReturnType result = (native.get()->*getter)();
          return JSIConverter<std::decay_t<ReturnType>>::toJSI(rt,
                                                               std::move(result));
        });

    auto setterFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, std::string("set_") + name),
        1,
        [setter](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          auto native = Derived::fromValue(rt, thisVal);
          auto value =
              JSIConverter<std::decay_t<ValueType>>::fromJSI(rt, args[0], false);
          (native.get()->*setter)(std::move(value));
          return jsi::Value::undefined();
        });

    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto defineProperty =
        objectCtor.getPropertyAsFunction(runtime, "defineProperty");

    jsi::Object descriptor(runtime);
    descriptor.setProperty(runtime, "get", getterFunc);
    descriptor.setProperty(runtime, "set", setterFunc);
    descriptor.setProperty(runtime, "enumerable", true);
    descriptor.setProperty(runtime, "configurable", true);

    defineProperty.call(runtime, prototype,
                        jsi::String::createFromUtf8(runtime, name), descriptor);
  }

private:
  // Helper to call a method with JSI argument conversion
  template <typename ReturnType, typename... Args, size_t... Is>
  static jsi::Value callMethod(Derived *obj,
                               ReturnType (Derived::*method)(Args...),
                               jsi::Runtime &runtime, const jsi::Value *args,
                               std::index_sequence<Is...>, size_t count) {
    if constexpr (std::is_same_v<ReturnType, void>) {
      (obj->*method)(JSIConverter<std::decay_t<Args>>::fromJSI(
          runtime, args[Is], Is >= count)...);
      return jsi::Value::undefined();
    } else if constexpr (std::is_same_v<ReturnType, jsi::Value>) {
      // Special case: if return type is jsi::Value, method has full control
      // This requires the method signature to match HostFunction
      return (obj->*method)(runtime, jsi::Value::undefined(), args, count);
    } else {
      ReturnType result = (obj->*method)(JSIConverter<std::decay_t<Args>>::fromJSI(
          runtime, args[Is], Is >= count)...);
      return JSIConverter<std::decay_t<ReturnType>>::toJSI(runtime,
                                                           std::move(result));
    }
  }
};

// Type trait to detect NativeObject-derived classes
template <typename T> struct is_native_object : std::false_type {};

template <typename T>
struct is_native_object<std::shared_ptr<T>>
    : std::bool_constant<std::is_base_of_v<NativeObject<T>, T>> {};

} // namespace margelo
