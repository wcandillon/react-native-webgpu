package com.webgpu;

import androidx.annotation.OptIn;

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
  private final Object mNativeLifecycleLock = new Object();
  private long mNativeSessionId = 0;

  static {
    System.loadLibrary("react-native-webgpu");
  }

  public WebGPUModule(ReactApplicationContext reactContext) {
    super(reactContext);
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean install() {
    synchronized (mNativeLifecycleLock) {
      if (mNativeSessionId != 0) {
        return true;
      }

      ReactApplicationContext context = getReactApplicationContext();
      JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
      CallInvokerHolder callInvokerHolder = context.getJSCallInvokerHolder();
      BlobModule blobModule = context.getNativeModule(BlobModule.class);

      if (jsContext == null) {
        throw new IllegalStateException("React Native's JavaScript context is not available");
      }
      if (!(callInvokerHolder instanceof CallInvokerHolderImpl)) {
        throw new IllegalStateException("React Native's JS CallInvoker is not available");
      }
      if (blobModule == null) {
        throw new IllegalStateException("React Native's BlobModule was not found");
      }

      // React Native clears this pointer under the same monitor during reload.
      // Keep it alive for the complete native installation transaction.
      synchronized (jsContext) {
        long jsRuntime = jsContext.get();
        if (jsRuntime == 0) {
          return false;
        }

        mNativeSessionId = initializeNative(
          jsRuntime,
          (CallInvokerHolderImpl) callInvokerHolder,
          blobModule
        );
      }

      return mNativeSessionId != 0;
    }
  }

  @Override
  public void invalidate() {
    synchronized (mNativeLifecycleLock) {
      if (mNativeSessionId != 0) {
        invalidateNative(mNativeSessionId);
        mNativeSessionId = 0;
      }
    }
    super.invalidate();
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @DoNotStrip
  private native long initializeNative(
    long jsRuntime,
    CallInvokerHolderImpl jsInvoker,
    BlobModule blobModule
  );

  @DoNotStrip
  private native void invalidateNative(long sessionId);
}
