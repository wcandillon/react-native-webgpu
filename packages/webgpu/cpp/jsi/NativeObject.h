//
// NativeObject base class for JSI NativeState pattern
//

#pragma once

#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "JSICache.h"
#include "WGPULogger.h"

// Forward declare to avoid circular dependency
namespace rnwgpu {
template <typename ArgType, typename SFINAE> struct JSIConverter;
} // namespace rnwgpu

// Include the converter - must come after forward declaration
#include "JSIConverter.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// Forward declaration
template <typename Derived> class NativeObject;

/**
 * Registry mapping a NativeObject class brand (CLASS_NAME) to a reconstructor
 * that rebuilds a fully functional JS object (prototype + native state) on
 * any runtime. This is what lets BoxedWebGPUObject::unbox() bring objects to
 * worklet runtimes.
 */
class NativeObjectRegistry {
public:
  using Reconstructor = std::function<jsi::Value(
      jsi::Runtime &, std::shared_ptr<jsi::NativeState>)>;

  static NativeObjectRegistry &getInstance() {
    static NativeObjectRegistry instance;
    return instance;
  }

  void registerClass(const std::string &brand, Reconstructor reconstructor) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_reconstructors.count(brand) != 0) {
      // The brand is the key unbox() uses to rebuild objects on worklet
      // runtimes - a duplicate would let one class hijack another's unboxing.
      throw std::runtime_error("Duplicate native object brand registered: " +
                               brand);
    }
    _reconstructors[brand] = std::move(reconstructor);
  }

  jsi::Value reconstruct(jsi::Runtime &runtime, const std::string &brand,
                         std::shared_ptr<jsi::NativeState> state) {
    Reconstructor reconstructor;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      auto it = _reconstructors.find(brand);
      if (it == _reconstructors.end()) {
        throw jsi::JSError(runtime,
                           "No native class registered for brand: " + brand);
      }
      reconstructor = it->second;
    }
    return reconstructor(runtime, std::move(state));
  }

private:
  NativeObjectRegistry() = default;
  std::mutex _mutex;
  std::unordered_map<std::string, Reconstructor> _reconstructors;
};

/**
 * BoxedWebGPUObject is a HostObject wrapper that holds a reference to ANY
 * WebGPU NativeObject. This is used for Reanimated/Worklets serialization.
 *
 * Since NativeObject uses NativeState (not HostObject), Worklets can't
 * serialize them directly. But Worklets CAN serialize HostObjects.
 *
 * This class stores:
 * - The NativeState from the original object
 * - The brand name for prototype reconstruction
 *
 * Usage pattern with registerCustomSerializable:
 * - pack(): Call WebGPU.box(obj) to create a BoxedWebGPUObject (HostObject)
 * - The HostObject is serialized by Worklets and transferred to UI runtime
 * - unpack(): Call boxed.unbox() to get the object's wrapper on that runtime
 *   (cached per runtime, see NativeObject::create), prototype included
 *
 * This is similar to NitroModules.box()/unbox() pattern.
 */
class BoxedWebGPUObject : public jsi::HostObject {
public:
  BoxedWebGPUObject(std::shared_ptr<jsi::NativeState> nativeState,
                    const std::string &brand)
      : _nativeState(std::move(nativeState)), _brand(brand) {}

  jsi::Value get(jsi::Runtime &runtime, const jsi::PropNameID &name) override {
    auto propName = name.utf8(runtime);
    if (propName == "unbox") {
      auto state = _nativeState;
      auto brand = _brand;
      return jsi::Function::createFromHostFunction(
          runtime, jsi::PropNameID::forUtf8(runtime, "unbox"), 0,
          [state, brand](jsi::Runtime &rt, const jsi::Value & /*thisVal*/,
                         const jsi::Value * /*args*/,
                         size_t /*count*/) -> jsi::Value {
            return NativeObjectRegistry::getInstance().reconstruct(rt, brand,
                                                                   state);
          });
    }
    if (propName == "__boxedWebGPU") {
      return jsi::Value(true);
    }
    if (propName == "__brand") {
      return jsi::String::createFromUtf8(runtime, _brand);
    }
    return jsi::Value::undefined();
  }

  void set(jsi::Runtime &runtime, const jsi::PropNameID &name,
           const jsi::Value &value) override {
    throw jsi::JSError(runtime, "BoxedWebGPUObject is read-only");
  }

  std::vector<jsi::PropNameID>
  getPropertyNames(jsi::Runtime &runtime) override {
    std::vector<jsi::PropNameID> names;
    names.reserve(3);
    names.push_back(jsi::PropNameID::forUtf8(runtime, "unbox"));
    names.push_back(jsi::PropNameID::forUtf8(runtime, "__boxedWebGPU"));
    names.push_back(jsi::PropNameID::forUtf8(runtime, "__brand"));
    return names;
  }

private:
  std::shared_ptr<jsi::NativeState> _nativeState;
  std::string _brand;
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
   * Key under which this class's prototype is stored in the per-runtime
   * JSICache: the C++ type of Derived.
   */
  static JSICache::PrototypeKey prototypeKey() {
    return std::type_index(typeid(Derived));
  }

  /**
   * Returns this class's prototype on `runtime`, or nullptr if it has not
   * been installed there yet. The prototype is owned by the runtime (see
   * JSICache), so the pointer is valid for as long as `runtime` is.
   */
  static jsi::Object *getCachedPrototype(jsi::Runtime &runtime) {
    return JSICache::get(runtime).getPrototype(prototypeKey());
  }

  /**
   * Ensure the prototype is installed for this runtime.
   * Called automatically by create(), but can be called manually.
   */
  static void installPrototype(jsi::Runtime &runtime) {
    if (getCachedPrototype(runtime) != nullptr) {
      return; // Already installed on this runtime
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

    // Install a generic toJSON so JSON.stringify works: data properties live
    // as getters on this shared prototype, and JSON.stringify only serializes
    // own enumerable properties (so it would otherwise produce {}).
    auto toJSON = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "toJSON"), 0,
        [](jsi::Runtime &rt, const jsi::Value &thisVal,
           const jsi::Value * /*args*/, size_t /*count*/) -> jsi::Value {
          auto thisObj = thisVal.getObject(rt);
          auto objectCtor = rt.global().getPropertyAsObject(rt, "Object");
          auto getPrototypeOf =
              objectCtor.getPropertyAsFunction(rt, "getPrototypeOf");
          auto proto = getPrototypeOf.call(rt, thisObj);
          jsi::Object result(rt);
          if (!proto.isObject()) {
            return std::move(result);
          }
          auto getOwnPropertyNames =
              objectCtor.getPropertyAsFunction(rt, "getOwnPropertyNames");
          auto names =
              getOwnPropertyNames.call(rt, proto).getObject(rt).getArray(rt);
          size_t length = names.size(rt);
          for (size_t i = 0; i < length; i++) {
            auto nameValue = names.getValueAtIndex(rt, i);
            if (!nameValue.isString()) {
              continue;
            }
            auto name = nameValue.getString(rt).utf8(rt);
            if (name == "constructor" || name == "toJSON") {
              continue;
            }
            // Read off `this` so prototype getters evaluate against the
            // object's native state. Getters that throw keep throwing.
            auto value = thisObj.getProperty(rt, name.c_str());
            if (value.isObject() && value.getObject(rt).isFunction(rt)) {
              // Skip methods - JSON.stringify would drop them anyway
              continue;
            }
            result.setProperty(rt, name.c_str(), value);
          }
          return std::move(result);
        });
    prototype.setProperty(runtime, "toJSON", toJSON);

    // Hand the prototype to the runtime-owned cache
    JSICache::get(runtime).setPrototype(prototypeKey(), std::move(prototype));
  }

  /**
   * Install a constructor function on the global object.
   * This enables `instanceof` checks: `obj instanceof ClassName`
   *
   * The constructor throws if called directly (these objects are only
   * created internally by the native code).
   *
   * Also registers this class with NativeObjectRegistry so that
   * BoxedWebGPUObject::unbox() can rebuild its objects on secondary runtimes.
   */
  static void installConstructor(jsi::Runtime &runtime) {
    // Register the reconstructor used by BoxedWebGPUObject::unbox() to
    // rebuild this object (prototype + native state) on another runtime.
    static std::once_flag registryFlag;
    std::call_once(registryFlag, []() {
      NativeObjectRegistry::getInstance().registerClass(
          Derived::CLASS_NAME,
          [](jsi::Runtime &rt,
             std::shared_ptr<jsi::NativeState> state) -> jsi::Value {
            auto instance = std::dynamic_pointer_cast<Derived>(state);
            if (instance == nullptr) {
              throw jsi::JSError(rt, "Invalid boxed native object state");
            }
            // The constructor makes `instanceof` work on this runtime too.
            if (!rt.global().hasProperty(rt, Derived::CLASS_NAME)) {
              Derived::installConstructor(rt);
            }
            // Unboxing hands back the wrapper cached for the target runtime,
            // creating it on first use. Keep the runtime the object was
            // originally created on: async native code (GPUDevice error/lost
            // events, GPUAdapter::requestDevice) delivers into the creation
            // runtime, and rebinding it to a worklet runtime would invoke
            // main-runtime jsi::Functions on the wrong runtime and thread.
            auto *originalRuntime = instance->getCreationRuntime();
            auto value = Derived::create(rt, instance);
            if (originalRuntime != nullptr) {
              instance->setCreationRuntime(originalRuntime);
            }
            return value;
          });
    });

    installPrototype(runtime);

    auto *prototype = getCachedPrototype(runtime);
    if (prototype == nullptr) {
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

    // Set the prototype property on the constructor
    // This is what makes `instanceof` work
    ctor.setProperty(runtime, "prototype", *prototype);

    // Set constructor property on prototype pointing back to constructor
    prototype->setProperty(runtime, "constructor", ctor);

    // Install on global
    runtime.global().setProperty(runtime, Derived::CLASS_NAME, std::move(ctor));
  }

  /**
   * Returns the JS wrapper of `instance` on `runtime`, creating it on first
   * use.
   *
   * A native object has at most one live wrapper per runtime. The wrapper is
   * cached (weakly, so it stays collectable) in the runtime's JSICache, and
   * converting the same object again (returning it from another native
   * call, unboxing it in a worklet on every frame, ...) hands back that
   * wrapper instead of a new one. Each wrapper reports the native memory the
   * object owns to the GC of its runtime through setExternalMemoryPressure,
   * so the memory is charged exactly once per runtime that can reach the
   * object, however often it is converted; charging every conversion would
   * multiply the amount by the number of captures in a worklet until Hermes
   * hit its max heap size. The hint is refreshed on every round trip for
   * objects whose size changes after creation.
   */
  static jsi::Value create(jsi::Runtime &runtime,
                           std::shared_ptr<Derived> instance) {
    auto &cache = JSICache::get(runtime);
    const void *key = instance.get();

    auto existing = cache.lockWrapper(runtime, key);
    if (existing.isObject()) {
      auto pressure = instance->getMemoryPressure();
      if (pressure > 0) {
        existing.getObject(runtime).setExternalMemoryPressure(runtime,
                                                              pressure);
      }
      return existing;
    }

    installPrototype(runtime);

    // Store creation runtime for logging etc.
    instance->setCreationRuntime(&runtime);

    // Create a new object
    jsi::Object obj(runtime);

    // Attach native state
    obj.setNativeState(runtime, instance);

    // Set prototype
    if (auto *prototype = getCachedPrototype(runtime)) {
      auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
      auto setPrototypeOf =
          objectCtor.getPropertyAsFunction(runtime, "setPrototypeOf");
      setPrototypeOf.call(runtime, obj, *prototype);
    }

    // Set memory pressure hint for GC
    auto pressure = instance->getMemoryPressure();
    if (pressure > 0) {
      obj.setExternalMemoryPressure(runtime, pressure);
    }

    cache.setWrapper(runtime, key, obj);
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
   * Install a method whose native implementation needs the calling jsi::Runtime
   * as its first parameter. Used by entry points that must act per-runtime
   * (e.g. GPU::requestAdapter, which creates a per-runtime RuntimeContext).
   */
  template <typename ReturnType, typename... Args>
  static void
  installMethodWithRuntime(jsi::Runtime &runtime, jsi::Object &prototype,
                           const char *name,
                           ReturnType (Derived::*method)(jsi::Runtime &,
                                                         Args...)) {
    auto func = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, name), sizeof...(Args),
        [method](jsi::Runtime &rt, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) -> jsi::Value {
          auto native = Derived::fromValue(rt, thisVal);
          return callMethodWithRuntime(native.get(), method, rt, args,
                                       std::index_sequence_for<Args...>{},
                                       count);
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
  // Helper to call a method that takes the calling jsi::Runtime as its first
  // parameter, with JSI argument conversion for the rest and JSI conversion of
  // the result.
  template <typename ReturnType, typename... Args, size_t... Is>
  static jsi::Value
  callMethodWithRuntime(Derived *obj,
                        ReturnType (Derived::*method)(jsi::Runtime &, Args...),
                        jsi::Runtime &runtime, const jsi::Value *args,
                        std::index_sequence<Is...>, size_t count) {
    ReturnType result = (obj->*method)(
        runtime, rnwgpu::JSIConverter<std::decay_t<Args>>::fromJSI(
                     runtime, args[Is], Is >= count)...);
    return rnwgpu::JSIConverter<std::decay_t<ReturnType>>::toJSI(
        runtime, std::move(result));
  }

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
