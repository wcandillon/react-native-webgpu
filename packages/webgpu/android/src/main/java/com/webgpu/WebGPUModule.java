package com.webgpu;

import android.util.Log;
import android.view.Choreographer;

import androidx.annotation.OptIn;

import java.util.HashSet;
import java.util.Set;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.modules.blob.BlobModule;
import com.facebook.react.modules.blob.BlobProvider;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;

@ReactModule(name = WebGPUModule.NAME)
public class WebGPUModule extends NativeWebGPUModuleSpec {
  static {
      System.loadLibrary("react-native-wgpu"); // Load the C++ library
  }

  private volatile boolean mTickActive = false;

  private final Choreographer.FrameCallback mFrameCallback = new Choreographer.FrameCallback() {
    @Override
    public void doFrame(long frameTimeNanos) {
      if (!mTickActive) {
        return;
      }
      nativeTick();
      Choreographer.getInstance().postFrameCallback(this);
    }
  };

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
    initializeNative(jsContext.get(), (CallInvokerHolderImpl) callInvokerHolder, blobModule);
    startVsyncTicks();
    return true;
  }

  @Override
  public void invalidate() {
    stopVsyncTicks();
    super.invalidate();
  }

  private void startVsyncTicks() {
    if (mTickActive) {
      return;
    }
    mTickActive = true;
    // Choreographer instances are per-thread; the FrameCallback fires on the
    // thread that posted it. Posting from the UI thread keeps the callback
    // there, which is required for Vulkan/Surface ops on Android.
    getReactApplicationContext().runOnUiQueueThread(new Runnable() {
      @Override
      public void run() {
        Choreographer.getInstance().postFrameCallback(mFrameCallback);
      }
    });
  }

  private void stopVsyncTicks() {
    mTickActive = false;
    getReactApplicationContext().runOnUiQueueThread(new Runnable() {
      @Override
      public void run() {
        Choreographer.getInstance().removeFrameCallback(mFrameCallback);
      }
    });
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker, BlobModule blobModule);

  @DoNotStrip
  private native void nativeTick();
}
