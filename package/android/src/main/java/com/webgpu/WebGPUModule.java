package com.webgpu;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;

public class WebGPUModule extends ReactContextBaseJavaModule {

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
        initializeNative();
    }

    private native void initializeNative();
}
