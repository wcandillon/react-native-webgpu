//
// NativeObject base class for JSI NativeState pattern
//

#pragma once

#include <cassert>
#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "RuntimeLifecycleMonitor.h"
#include "WGPULogger.h"

// Forward declare to avoid circular dependency
namespace rnwgpu {
template <typename ArgType, typename SFINAE> struct JSIConverter;
} // namespace rnwgpu

// Include the converter - must come after forward declaration
#include "JSIConverter.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

/**
 * Per-runtime cache entry for a prototype object.
 * Uses std::optional<jsi::Object> so the prototype is stored directly
 * without extra indirection.
 */
struct PrototypeCacheEntry {
  std::optional<jsi::Object> prototype;
};

/**
 * Base class for runtime-aware caches. Provides static storage for the
 * main runtime pointer, which must be set during initialization.
 */
class BaseRuntimeAwareCache {
public:
  static void setMainJsRuntime(jsi::Runtime *rt) { _mainRuntime = rt; }

protected:
  static jsi::Runtime *getMainJsRuntime() {
    assert(_mainRuntime != nullptr &&
           "Expected main Javascript runtime to be set in the "
           "BaseRuntimeAwareCache class.");
    return _mainRuntime;
  }

private:
  static jsi::Runtime *_mainRuntime;
};

/**
 * Runtime-aware cache that stores data per-runtime and automatically
 * cleans up when a runtime is destroyed.
 *
 * This follows the same pattern as React Native Skia's RuntimeAwareCache:
 * - For the primary/main runtime: uses a simple member variable (zero overhead)
 * - For secondary runtimes: tracks lifecycle and auto-cleans on destruction
 *
 * The assumption is that the main runtime outlives the cache itself,
 * so we don't need to register for its destruction events.
 */
template <typename T>
class RuntimeAwareCache : public BaseRuntimeAwareCache,
                          public RuntimeLifecycleListener {
public:
  void onRuntimeDestroyed(jsi::Runtime *rt) override {
    if (getMainJsRuntime() != rt) {
      // We are removing a secondary runtime
      _secondaryRuntimeCaches.erase(rt);
    }
  }

  ~RuntimeAwareCache() {
    for (auto &cache : _secondaryRuntimeCaches) {
      RuntimeLifecycleMonitor::removeListener(
          *static_cast<jsi::Runtime *>(cache.first), this);
    }
  }

  T &get(jsi::Runtime &rt) {
    // We check if we're accessing the main runtime - this is the happy path
    // to avoid us having to lookup by runtime for caches that only has a single
    // runtime
    if (getMainJsRuntime() == &rt) {
      return _primaryCache;
    } else {
      if (_secondaryRuntimeCaches.count(&rt) == 0) {
        // We only add listener when the secondary runtime is used, this assumes
        // that the secondary runtime is terminated first. This lets us avoid
        // additional complexity for the majority of cases when objects are not
        // shared between runtimes. Otherwise we'd have to register all objects
        // with the RuntimeMonitor as opposed to only registering ones that are
        // used in secondary runtime. Note that we can't register listener here
        // with the primary runtime as it may run on a separate thread.
        RuntimeLifecycleMonitor::addListener(rt, this);

        T cache;
        _secondaryRuntimeCaches.emplace(&rt, std::move(cache));
      }
    }
    return _secondaryRuntimeCaches.at(&rt);
  }

private:
  std::unordered_map<void *, T> _secondaryRuntimeCaches;
  T _primaryCache;
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
   * Get the prototype cache for this type.
   * Each NativeObject<Derived> type has its own static cache.
   * Uses RuntimeAwareCache to properly handle runtime lifecycle.
   */
  static RuntimeAwareCache<PrototypeCacheEntry> &getPrototypeCache() {
    static RuntimeAwareCache<PrototypeCacheEntry> cache;
    return cache;
  }

  /**
   * Ensure the prototype is installed for this runtime.
   * Called automatically by create(), but can be called manually.
   */
  static void installPrototype(jsi::Runtime &runtime) {
    auto &entry = getPrototypeCache().get(runtime);
    if (entry.prototype.has_value()) {
      return; // Already installed
    }

    // Create prototype object
    jsi::Object prototype(runtime);

    // Let derived class define its methods/properties
    Derived::definePrototype(runtime, prototype);

    // Add Symbol.toStringTag for proper object identification in console.log
    auto symbolCtor = runtime.global().getPropertyAsObject(runtime, "Symbol");
    auto toStringTag = symbolCtor.getProperty(runtime, "toStringTag");
    if (!toStringTag.isUndefined()) {
      // Use Object.defineProperty to set symbol property since setProperty
      // doesn't support symbols directly
      auto objectCtor =
          runtime.global().getPropertyAsObject(runtime, "Object");
      auto defineProperty =
          objectCtor.getPropertyAsFunction(runtime, "defineProperty");
      jsi::Object descriptor(runtime);
      descriptor.setProperty(
          runtime, "value",
          jsi::String::createFromUtf8(runtime, Derived::CLASS_NAME));
      descriptor.setProperty(runtime, "writable", false);
      descriptor.setProperty(runtime, "enumerable", false);
      descriptor.setProperty(runtime, "configurable", true);
      defineProperty.call(runtime, prototype, toStringTag, descriptor);
    }

    // Cache the prototype
    entry.prototype = std::move(prototype);
  }

  /**
   * Install a constructor function on the global object.
   * This enables `instanceof` checks: `obj instanceof ClassName`
   *
   * The constructor throws if called directly (these objects are only
   * created internally by the native code).
   *
   * Since we don't use prototype chain inheritance (to support Reanimated),
   * we implement `instanceof` via Symbol.hasInstance which checks the __brand
   * property instead.
   */
  static void installConstructor(jsi::Runtime &runtime) {
    installPrototype(runtime);

    auto &entry = getPrototypeCache().get(runtime);
    if (!entry.prototype.has_value()) {
      return;
    }

    // Create a constructor function that throws when called directly
    auto ctor = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, Derived::CLASS_NAME), 0,
        [](jsi::Runtime &rt, const jsi::Value & /*thisVal*/,
           const jsi::Value * /*args*/, size_t /*count*/) -> jsi::Value {
          throw jsi::JSError(
              rt, std::string("Illegal constructor: ") + Derived::CLASS_NAME +
                      " objects are created by the WebGPU API");
        });

    // Set the prototype property on the constructor (for consistency)
    ctor.setProperty(runtime, "prototype", *entry.prototype);

    // Set constructor property on prototype pointing back to constructor
    entry.prototype->setProperty(runtime, "constructor", ctor);

    // Implement Symbol.hasInstance for instanceof support
    // Since we copy properties instead of using setPrototypeOf, we need
    // custom instanceof behavior that checks the __brand property
    auto symbolCtor = runtime.global().getPropertyAsObject(runtime, "Symbol");
    auto hasInstance = symbolCtor.getProperty(runtime, "hasInstance");
    if (!hasInstance.isUndefined()) {
      auto hasInstanceFunc = jsi::Function::createFromHostFunction(
          runtime,
          jsi::PropNameID::forUtf8(runtime,
                                   std::string("[Symbol.hasInstance]")),
          1,
          [](jsi::Runtime &rt, const jsi::Value & /*thisVal*/,
             const jsi::Value *args, size_t count) -> jsi::Value {
            if (count < 1 || !args[0].isObject()) {
              return jsi::Value(false);
            }
            auto obj = args[0].getObject(rt);
            auto brand = obj.getProperty(rt, "__brand");
            if (!brand.isString()) {
              return jsi::Value(false);
            }
            return jsi::Value(brand.getString(rt).utf8(rt) ==
                              Derived::CLASS_NAME);
          });

      // Use Object.defineProperty to set the symbol property
      auto objectCtor =
          runtime.global().getPropertyAsObject(runtime, "Object");
      auto defineProperty =
          objectCtor.getPropertyAsFunction(runtime, "defineProperty");
      jsi::Object descriptor(runtime);
      descriptor.setProperty(runtime, "value", hasInstanceFunc);
      descriptor.setProperty(runtime, "writable", false);
      descriptor.setProperty(runtime, "enumerable", false);
      descriptor.setProperty(runtime, "configurable", true);
      defineProperty.call(runtime, ctor, hasInstance, descriptor);
    }

    // Install on global
    runtime.global().setProperty(runtime, Derived::CLASS_NAME, std::move(ctor));
  }

  /**
   * Create a JS object with native state attached.
   *
   * Instead of using Object.setPrototypeOf (which would change the prototype
   * chain and break Reanimated's isPlainJSObject check), we copy all properties
   * from the cached prototype directly onto the new object. This ensures:
   * 1. Object.getPrototypeOf(obj) === Object.prototype (passes isPlainJSObject)
   * 2. The object still has all WebGPU methods and getters
   * 3. The native state is preserved when serialized by Reanimated/Worklets
   */
  static jsi::Value create(jsi::Runtime &runtime,
                           std::shared_ptr<Derived> instance) {
    installPrototype(runtime);

    // Store creation runtime for logging etc.
    instance->setCreationRuntime(&runtime);

    // Create a new object
    jsi::Object obj(runtime);

    // Attach native state
    obj.setNativeState(runtime, instance);

    // Copy all properties from the prototype to the object
    // This keeps Object.prototype as the prototype (important for Reanimated)
    // while still giving the object all its methods and getters
    auto &entry = getPrototypeCache().get(runtime);
    if (entry.prototype.has_value()) {
      auto objectCtor =
          runtime.global().getPropertyAsObject(runtime, "Object");

      // Get all property names from prototype (including non-enumerable)
      auto getOwnPropertyNames =
          objectCtor.getPropertyAsFunction(runtime, "getOwnPropertyNames");
      auto getOwnPropertyDescriptor =
          objectCtor.getPropertyAsFunction(runtime, "getOwnPropertyDescriptor");
      auto defineProperty =
          objectCtor.getPropertyAsFunction(runtime, "defineProperty");

      auto names =
          getOwnPropertyNames.call(runtime, *entry.prototype).asObject(runtime);
      auto namesArray = names.asArray(runtime);
      size_t length = namesArray.size(runtime);

      for (size_t i = 0; i < length; i++) {
        auto name = namesArray.getValueAtIndex(runtime, i);
        // Skip 'constructor' property
        if (name.isString() &&
            name.getString(runtime).utf8(runtime) == "constructor") {
          continue;
        }
        // Get the property descriptor and define it on the new object
        auto descriptor =
            getOwnPropertyDescriptor.call(runtime, *entry.prototype, name);
        if (!descriptor.isUndefined()) {
          defineProperty.call(runtime, obj, name, descriptor);
        }
      }
    }

    // Set memory pressure hint for GC
    auto pressure = instance->getMemoryPressure();
    if (pressure > 0) {
      obj.setExternalMemoryPressure(runtime, pressure);
    }

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

  /**
   * Set the creation runtime. Called during create().
   */
  void setCreationRuntime(jsi::Runtime *runtime) { _creationRuntime = runtime; }

  /**
   * Get the creation runtime.
   * WARNING: This pointer may become invalid if the runtime is destroyed.
   */
  jsi::Runtime *getCreationRuntime() const { return _creationRuntime; }

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
            return rnwgpu::JSIConverter<std::decay_t<ReturnType>>::toJSI(
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
          if (count < 1) {
            throw jsi::JSError(rt, "Setter requires a value argument");
          }
          auto native = Derived::fromValue(rt, thisVal);
          auto value =
              rnwgpu::JSIConverter<std::decay_t<ValueType>>::fromJSI(rt, args[0], false);
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
          return rnwgpu::JSIConverter<std::decay_t<ReturnType>>::toJSI(rt,
                                                               std::move(result));
        });

    auto setterFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, std::string("set_") + name),
        1,
        [setter](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          if (count < 1) {
            throw jsi::JSError(rt, "Setter requires a value argument");
          }
          auto native = Derived::fromValue(rt, thisVal);
          auto value =
              rnwgpu::JSIConverter<std::decay_t<ValueType>>::fromJSI(rt, args[0], false);
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
      (obj->*method)(rnwgpu::JSIConverter<std::decay_t<Args>>::fromJSI(
          runtime, args[Is], Is >= count)...);
      return jsi::Value::undefined();
    } else if constexpr (std::is_same_v<ReturnType, jsi::Value>) {
      // Special case: if return type is jsi::Value, method has full control
      // This requires the method signature to match HostFunction
      return (obj->*method)(runtime, jsi::Value::undefined(), args, count);
    } else {
      ReturnType result = (obj->*method)(rnwgpu::JSIConverter<std::decay_t<Args>>::fromJSI(
          runtime, args[Is], Is >= count)...);
      return rnwgpu::JSIConverter<std::decay_t<ReturnType>>::toJSI(runtime,
                                                           std::move(result));
    }
  }
};

// Type trait to detect NativeObject-derived classes
template <typename T> struct is_native_object : std::false_type {};

template <typename T>
struct is_native_object<std::shared_ptr<T>>
    : std::bool_constant<std::is_base_of_v<NativeObject<T>, T>> {};

} // namespace rnwgpu
