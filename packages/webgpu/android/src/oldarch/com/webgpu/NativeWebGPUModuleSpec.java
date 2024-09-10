package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;

class NativeWebGPUModuleSpec extends ReactContextBaseJavaModule {

  public static final String NAME = "WebGPUModule";

  public NativeWebGPUModuleSpec(@Nullable ReactApplicationContext reactContext) {
    super(reactContext);
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

}
