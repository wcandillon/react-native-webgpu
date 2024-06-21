package com.webgpu;

import com.facebook.jni.HybridData;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;

@FrameworkAPI public class WebGPUModule extends ReactContextBaseJavaModule {
    static {
        System.loadLibrary("react-native-webgpu"); // Load the C++ library
    }

    public WebGPUModule(ReactApplicationContext reactContext) {
        super(reactContext);
        // Initialize the C++ module
        initialize();
    }

    @Override
    public String getName() {
        return "WebGPUModule";
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    public void install() {
      ReactApplicationContext context = getReactApplicationContext();
      JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
      CallInvokerHolder callInvokerHolder = context.getCatalystInstance().getJSCallInvokerHolder();
      initializeNative(jsContext.get(),  (CallInvokerHolderImpl) callInvokerHolder);
    }

    @DoNotStrip
    private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker);
}
