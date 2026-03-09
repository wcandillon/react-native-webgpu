package com.webgpu;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.OptIn;

import java.util.ArrayList;
import java.util.List;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMethod;
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
  public boolean install() {
    ReactApplicationContext context = getReactApplicationContext();
    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
    CallInvokerHolder callInvokerHolder = context.getCatalystInstance().getJSCallInvokerHolder();
    BlobModule blobModule = getReactApplicationContext().getNativeModule(BlobModule.class);
    if (blobModule == null) {
      throw new RuntimeException("React Native's BlobModule was not found!");
    }

    String[] enableToggles = readToggleMetadata("com.webgpu.enable_toggles");
    String[] disableToggles = readToggleMetadata("com.webgpu.disable_toggles");

    initializeNative(jsContext.get(), (CallInvokerHolderImpl) callInvokerHolder, blobModule,
        enableToggles, disableToggles);
    return true;
  }

  private String[] readToggleMetadata(String key) {
    Bundle metadata = getApplicationMetadata();
    if (metadata == null) {
      return new String[0];
    }
    return parseToggleList(metadata.getString(key));
  }

  @Nullable
  private Bundle getApplicationMetadata() {
    ReactApplicationContext context = getReactApplicationContext();
    try {
      ApplicationInfo appInfo = context.getPackageManager().getApplicationInfo(
          context.getPackageName(), PackageManager.GET_META_DATA);
      return appInfo.metaData;
    } catch (PackageManager.NameNotFoundException e) {
      return null;
    }
  }

  private static String[] parseToggleList(@Nullable String rawValue) {
    if (rawValue == null || rawValue.trim().isEmpty()) {
      return new String[0];
    }

    List<String> toggles = new ArrayList<>();
    String[] parts = rawValue.split(",");
    for (String part : parts) {
      String toggle = part.trim();
      if (!toggle.isEmpty()) {
        toggles.add(toggle);
      }
    }
    return toggles.toArray(new String[0]);
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker,
      BlobModule blobModule, String[] enableToggles, String[] disableToggles);
}
