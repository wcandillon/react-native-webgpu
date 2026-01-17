#pragma once

#include <memory>
#include <string>

#include "NativeObject.h"

#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct Blob {
  std::string blobId;
  double size;
  double offset;
  std::string type;
  std::string name;
};

class RNWebGPU : public NativeObject<RNWebGPU> {
public:
  static constexpr const char *CLASS_NAME = "RNWebGPU";

  explicit RNWebGPU(std::shared_ptr<GPU> gpu,
                    std::shared_ptr<PlatformContext> platformContext)
      : NativeObject(CLASS_NAME), _gpu(gpu), _platformContext(platformContext) {
  }

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  /**
   * Box any WebGPU NativeObject into a HostObject for Worklets serialization.
   *
   * This function checks if the object has native state and a prototype with
   * Symbol.toStringTag (which identifies the WebGPU class name), then creates
   * a BoxedWebGPUObject that can be serialized by Worklets.
   *
   * Usage with registerCustomSerializable:
   *   pack: (value) => { 'worklet'; return RNWebGPU.box(value); }
   *   unpack: (boxed) => { 'worklet'; return boxed.unbox(); }
   */
  jsi::Value box(jsi::Runtime &runtime, const jsi::Value &thisVal,
                 const jsi::Value *args, size_t count) {
    if (count < 1 || !args[0].isObject()) {
      throw jsi::JSError(runtime, "box() requires a WebGPU object argument");
    }

    auto obj = args[0].getObject(runtime);

    // Check if it has native state
    if (!obj.hasNativeState(runtime)) {
      throw jsi::JSError(runtime, "Object has no native state - not a WebGPU object");
    }

    // Get the brand name from Symbol.toStringTag on the prototype
    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto getPrototypeOf =
        objectCtor.getPropertyAsFunction(runtime, "getPrototypeOf");
    auto proto = getPrototypeOf.call(runtime, obj);

    std::string brand;
    if (proto.isObject()) {
      auto protoObj = proto.getObject(runtime);
      auto symbolCtor = runtime.global().getPropertyAsObject(runtime, "Symbol");
      auto toStringTag = symbolCtor.getProperty(runtime, "toStringTag");
      if (!toStringTag.isUndefined()) {
        // Get the Symbol.toStringTag property value from the prototype
        auto getOwnPropertyDescriptor =
            objectCtor.getPropertyAsFunction(runtime, "getOwnPropertyDescriptor");
        auto desc = getOwnPropertyDescriptor.call(runtime, protoObj, toStringTag);
        if (desc.isObject()) {
          auto descObj = desc.getObject(runtime);
          auto value = descObj.getProperty(runtime, "value");
          if (value.isString()) {
            brand = value.getString(runtime).utf8(runtime);
          }
        }
      }
    }

    if (brand.empty()) {
      throw jsi::JSError(
          runtime, "Cannot determine WebGPU object type - no Symbol.toStringTag found");
    }

    auto nativeState = obj.getNativeState(runtime);
    auto boxed = std::make_shared<BoxedWebGPUObject>(nativeState, brand);
    return jsi::Object::createFromHostObject(runtime, boxed);
  }

  /**
   * Check if a value is a boxed WebGPU object (created by box())
   */
  jsi::Value isBoxedWebGPUObject(jsi::Runtime &runtime, const jsi::Value &thisVal,
                                  const jsi::Value *args, size_t count) {
    if (count < 1 || !args[0].isObject()) {
      return jsi::Value(false);
    }
    auto obj = args[0].getObject(runtime);
    if (!obj.isHostObject(runtime)) {
      return jsi::Value(false);
    }
    // Check for __boxedWebGPU marker
    auto marker = obj.getProperty(runtime, "__boxedWebGPU");
    return jsi::Value(marker.isBool() && marker.getBool());
  }

  /**
   * Check if a value is a WebGPU NativeObject (has native state and WebGPU prototype)
   */
  jsi::Value isWebGPUObject(jsi::Runtime &runtime, const jsi::Value &thisVal,
                            const jsi::Value *args, size_t count) {
    if (count < 1 || !args[0].isObject()) {
      return jsi::Value(false);
    }
    auto obj = args[0].getObject(runtime);

    // Check if it has native state
    if (!obj.hasNativeState(runtime)) {
      return jsi::Value(false);
    }

    // Check if it has Symbol.toStringTag on its prototype (WebGPU objects do)
    auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
    auto getPrototypeOf =
        objectCtor.getPropertyAsFunction(runtime, "getPrototypeOf");
    auto proto = getPrototypeOf.call(runtime, obj);

    if (!proto.isObject()) {
      return jsi::Value(false);
    }

    auto protoObj = proto.getObject(runtime);
    auto symbolCtor = runtime.global().getPropertyAsObject(runtime, "Symbol");
    auto toStringTag = symbolCtor.getProperty(runtime, "toStringTag");
    if (toStringTag.isUndefined()) {
      return jsi::Value(false);
    }

    auto getOwnPropertyDescriptor =
        objectCtor.getPropertyAsFunction(runtime, "getOwnPropertyDescriptor");
    auto desc = getOwnPropertyDescriptor.call(runtime, protoObj, toStringTag);
    return jsi::Value(desc.isObject());
  }

  bool getFabric() { return true; }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(int contextId, float width, float height) {
    auto ctx =
        std::make_shared<GPUCanvasContext>(_gpu, contextId, width, height);
    return ctx;
  }

  std::shared_ptr<ImageBitmap> createImageBitmap(std::shared_ptr<Blob> blob) {
    auto imageData = _platformContext->createImageBitmap(
        blob->blobId, blob->offset, blob->size);
    auto imageBitmap = std::make_shared<ImageBitmap>(imageData);
    return imageBitmap;
  }

  std::shared_ptr<Canvas> getNativeSurface(int contextId) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurfaceInfo(contextId);
    if (info == nullptr) {
      return std::make_shared<Canvas>(nullptr, 0, 0);
    }
    auto nativeInfo = info->getNativeInfo();
    return std::make_shared<Canvas>(nativeInfo.nativeSurface, nativeInfo.width,
                                    nativeInfo.height);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "fabric", &RNWebGPU::getFabric);
    installGetter(runtime, prototype, "gpu", &RNWebGPU::getGPU);
    installMethod(runtime, prototype, "createImageBitmap",
                  &RNWebGPU::createImageBitmap);
    installMethod(runtime, prototype, "getNativeSurface",
                  &RNWebGPU::getNativeSurface);
    installMethod(runtime, prototype, "MakeWebGPUCanvasContext",
                  &RNWebGPU::MakeWebGPUCanvasContext);

    // Install box() - boxes a WebGPU object for Worklets serialization
    auto boxFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "box"), 1,
        [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args,
           size_t count) -> jsi::Value {
          auto native = RNWebGPU::fromValue(rt, thisVal);
          return native->box(rt, thisVal, args, count);
        });
    prototype.setProperty(runtime, "box", boxFunc);

    // Install isWebGPUObject() - checks if a value is a WebGPU NativeObject
    auto isWebGPUObjectFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "isWebGPUObject"), 1,
        [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args,
           size_t count) -> jsi::Value {
          auto native = RNWebGPU::fromValue(rt, thisVal);
          return native->isWebGPUObject(rt, thisVal, args, count);
        });
    prototype.setProperty(runtime, "isWebGPUObject", isWebGPUObjectFunc);

    // Install isBoxedWebGPUObject() - checks if a value is a boxed WebGPU object
    auto isBoxedWebGPUObjectFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "isBoxedWebGPUObject"), 1,
        [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args,
           size_t count) -> jsi::Value {
          auto native = RNWebGPU::fromValue(rt, thisVal);
          return native->isBoxedWebGPUObject(rt, thisVal, args, count);
        });
    prototype.setProperty(runtime, "isBoxedWebGPUObject", isBoxedWebGPUObjectFunc);
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

template <> struct JSIConverter<std::shared_ptr<Blob>> {
  static std::shared_ptr<Blob>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    if (!outOfBounds && arg.isObject()) {
      auto result = std::make_unique<Blob>();
      auto val = arg.asObject(runtime);
      if (val.hasProperty(runtime, "_data")) {
        auto value = val.getPropertyAsObject(runtime, "_data");
        result->blobId = JSIConverter<std::string>::fromJSI(
            runtime, value.getProperty(runtime, "blobId"), false);
        //        result->type = JSIConverter<std::string>::fromJSI(
        //            runtime, value.getProperty(runtime, "type"), false);
        //        result->name = JSIConverter<std::string>::fromJSI(
        //            runtime, value.getProperty(runtime, "name"), false);
        result->size = JSIConverter<double>::fromJSI(
            runtime, value.getProperty(runtime, "size"), false);
        result->offset = JSIConverter<double>::fromJSI(
            runtime, value.getProperty(runtime, "offset"), false);
      }
      return result;
    } else {
      throw std::runtime_error("Invalid Blob::fromJSI()");
    }
  }
  static jsi::Value toJSI(jsi::Runtime &runtime, std::shared_ptr<Blob> arg) {
    throw std::runtime_error("Invalid Blob::toJSI()");
  }
};

} // namespace rnwgpu
