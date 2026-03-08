package com.webgpu;

import android.util.Log;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;

import java.util.ArrayList;
import java.util.List;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMap;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.modules.blob.BlobModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;

@ReactModule(name = WebGPUModule.NAME)
public class WebGPUModule extends NativeWebGPUModuleSpec {
  static {
      System.loadLibrary("react-native-wgpu"); // Load the C++ library
  }

  public WebGPUModule(ReactApplicationContext reactContext) {
    super(reactContext);
    // Initialize the C++ module
    initialize();
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean install(@Nullable ReadableMap options) {
    ReactApplicationContext context = getReactApplicationContext();
    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
    CallInvokerHolder callInvokerHolder = context.getCatalystInstance().getJSCallInvokerHolder();
    BlobModule blobModule = getReactApplicationContext().getNativeModule(BlobModule.class);
    if (blobModule == null) {
      throw new RuntimeException("React Native's BlobModule was not found!");
    }

    // Extract dawnToggles from options
    String[] enableToggles = new String[0];
    String[] disableToggles = new String[0];
    if (options != null && options.hasKey("dawnToggles")) {
      ReadableMap dawnToggles = options.getMap("dawnToggles");
      if (dawnToggles != null) {
        if (dawnToggles.hasKey("enable")) {
          ReadableArray enableArray = dawnToggles.getArray("enable");
          if (enableArray != null) {
            List<String> list = new ArrayList<>();
            for (int i = 0; i < enableArray.size(); i++) {
              list.add(enableArray.getString(i));
            }
            enableToggles = list.toArray(new String[0]);
          }
        }
        if (dawnToggles.hasKey("disable")) {
          ReadableArray disableArray = dawnToggles.getArray("disable");
          if (disableArray != null) {
            List<String> list = new ArrayList<>();
            for (int i = 0; i < disableArray.size(); i++) {
              list.add(disableArray.getString(i));
            }
            disableToggles = list.toArray(new String[0]);
          }
        }
      }
    }

    initializeNative(jsContext.get(), (CallInvokerHolderImpl) callInvokerHolder, blobModule,
        enableToggles, disableToggles);
    return true;
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker,
      BlobModule blobModule, String[] enableToggles, String[] disableToggles);
}
