//
//  WebGPUObject.hpp
//  Pods
//
//  Created by Ritesh Shukla on 29/09/26.
//

#pragma once

#include <string>

#include <NitroModules/HybridObject.hpp>
#include <NitroModules/Prototype.hpp>
#include <jsi/jsi.h>

namespace rnwgpu {

namespace jsi = facebook::jsi;
using margelo::nitro::HybridObject;
using margelo::nitro::Prototype;

// Base for every WebGPU HybridObject: adds today's `__brand`, `toJSON` and
// `toString()` on top of the nitrogen-generated `Spec`.
template <typename Spec> class WebGPUObject : public Spec {
public:
  std::string toString() override { return "[object " + this->getName() + "]"; }

protected:
  size_t getExternalMemorySize() noexcept override { return 1024; }

  void loadHybridMethods() override {
    Spec::loadHybridMethods();
    this->registerHybrids(this, [](Prototype &prototype) {
      prototype.registerHybridGetter("__brand", &HybridObject::getName);
      prototype.registerRawHybridMethod("toJSON", 0, &WebGPUObject::toJSON);
    });
  }

private:
  jsi::Value toJSON(jsi::Runtime &runtime, const jsi::Value &thisValue,
                    const jsi::Value * /*args*/, size_t /*count*/) {
    auto self = thisValue.asObject(runtime);
    auto objectConstructor =
        runtime.global().getPropertyAsObject(runtime, "Object");
    auto getPrototypeOf =
        objectConstructor.getPropertyAsFunction(runtime, "getPrototypeOf");
    auto getOwnPropertyNames =
        objectConstructor.getPropertyAsFunction(runtime, "getOwnPropertyNames");
    auto objectPrototype = objectConstructor.getProperty(runtime, "prototype");

    jsi::Object json(runtime);
    for (auto prototype = getPrototypeOf.call(runtime, self);
         prototype.isObject() &&
         !jsi::Value::strictEquals(runtime, prototype, objectPrototype);
         prototype = getPrototypeOf.call(runtime, prototype)) {
      auto names = getOwnPropertyNames.call(runtime, prototype)
                       .asObject(runtime)
                       .asArray(runtime);
      for (size_t i = 0; i < names.size(runtime); i++) {
        auto name =
            names.getValueAtIndex(runtime, i).asString(runtime).utf8(runtime);
        if (isSkipped(name) || json.hasProperty(runtime, name.c_str())) {
          continue;
        }
        auto value = self.getProperty(runtime, name.c_str());
        if (value.isObject() && value.asObject(runtime).isFunction(runtime)) {
          continue;
        }
        json.setProperty(runtime, name.c_str(), value);
      }
    }
    return json;
  }

  static bool isSkipped(const std::string &name) {
    return name == "constructor" || name == "toJSON" || name == "name";
  }
};

} // namespace rnwgpu
