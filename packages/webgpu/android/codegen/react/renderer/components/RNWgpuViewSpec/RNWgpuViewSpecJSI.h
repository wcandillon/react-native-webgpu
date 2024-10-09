#pragma once

#include <ReactCommon/TurboModule.h>
#include <react/bridging/Bridging.h>

namespace facebook::react {


  class JSI_EXPORT NativeWebGPUModuleCxxSpecJSI : public TurboModule {
protected:
  NativeWebGPUModuleCxxSpecJSI(std::shared_ptr<CallInvoker> jsInvoker);

public:
  virtual bool install(jsi::Runtime &rt) = 0;
  virtual bool createSurfaceContext(jsi::Runtime &rt, double contextId) = 0;

};

template <typename T>
class JSI_EXPORT NativeWebGPUModuleCxxSpec : public TurboModule {
public:
  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propName) override {
    return delegate_.get(rt, propName);
  }

  static constexpr std::string_view kModuleName = "WebGPUModule";

protected:
  NativeWebGPUModuleCxxSpec(std::shared_ptr<CallInvoker> jsInvoker)
    : TurboModule(std::string{NativeWebGPUModuleCxxSpec::kModuleName}, jsInvoker),
      delegate_(reinterpret_cast<T*>(this), jsInvoker) {}

private:
  class Delegate : public NativeWebGPUModuleCxxSpecJSI {
  public:
    Delegate(T *instance, std::shared_ptr<CallInvoker> jsInvoker) :
      NativeWebGPUModuleCxxSpecJSI(std::move(jsInvoker)), instance_(instance) {}

    bool install(jsi::Runtime &rt) override {
      static_assert(
          bridging::getParameterCount(&T::install) == 1,
          "Expected install(...) to have 1 parameters");

      return bridging::callFromJs<bool>(
          rt, &T::install, jsInvoker_, instance_);
    }
    bool createSurfaceContext(jsi::Runtime &rt, double contextId) override {
      static_assert(
          bridging::getParameterCount(&T::createSurfaceContext) == 2,
          "Expected createSurfaceContext(...) to have 2 parameters");

      return bridging::callFromJs<bool>(
          rt, &T::createSurfaceContext, jsInvoker_, instance_, std::move(contextId));
    }

  private:
    T *instance_;
  };

  Delegate delegate_;
};

} // namespace facebook::react
