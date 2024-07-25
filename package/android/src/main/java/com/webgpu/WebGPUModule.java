package com.webgpu;

import android.util.Log;
import java.util.HashSet;
import java.util.Set;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;

@ReactModule(name = WebGPUModule.NAME)
public class WebGPUModule extends ReactContextBaseJavaModule {
  static {
      System.loadLibrary("react-native-webgpu"); // Load the C++ library
  }

  public static final String NAME = "WebGPUModule";
  private final Object contextLock = new Object();
  private final Set<Integer> surfaceContextsIds = new HashSet<>();

  public WebGPUModule(ReactApplicationContext reactContext) {
      super(reactContext);
      // Initialize the C++ module
      initialize();
  }

  @Override
  public String getName() {
      return NAME;
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void install() {
    ReactApplicationContext context = getReactApplicationContext();
    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
    CallInvokerHolder callInvokerHolder = context.getCatalystInstance().getJSCallInvokerHolder();
    initializeNative(jsContext.get(), (CallInvokerHolderImpl) callInvokerHolder);
  }

  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker);

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void createSurfaceContext(Integer contextId) {
    waitForNativeSurface(contextId);

    ReactApplicationContext context = getReactApplicationContext();
    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
    createSurfaceContext(jsContext.get(), contextId);
  }

  @DoNotStrip
  private native void createSurfaceContext(long jsRuntime, int contextId);

  private void waitForNativeSurface(Integer contextId) {
    synchronized (contextLock) {
      while (!surfaceContextsIds.contains(contextId)) {
        try {
          contextLock.wait();
        } catch (InterruptedException e) {
          Log.e("RNWebGPU", "Unable to create a context");
          return;
        }
      }
    }
  }

  protected void onSurfaceCreated(Integer contextId) {
    synchronized (contextLock) {
      surfaceContextsIds.add(contextId);
      contextLock.notifyAll();
    }
  }

}
