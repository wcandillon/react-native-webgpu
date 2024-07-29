package com.webgpu;

import android.util.Log;
import java.util.HashSet;
import java.util.Set;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;

@ReactModule(name = WebGPUModule.NAME)
public class WebGPUModule extends NativeWebGPUModuleSpec {
  static {
      System.loadLibrary("react-native-webgpu"); // Load the C++ library
  }

  public static WebGPUModule instance;

  public static final String NAME = "WebGPUModule";
  private final Object contextLock = new Object();
  private final Set<Integer> surfaceContextsIds = new HashSet<>();
  ReactApplicationContext reactContext;

  public WebGPUModule(ReactApplicationContext reactContext) {
      super(reactContext);
      // Initialize the C++ module
      initialize();
    this.reactContext = reactContext;
    instance = this;
  }

//  @Override
  public String getName() {
      return NAME;
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean install() {
    ReactApplicationContext context = getReactApplicationContext();
    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
    CallInvokerHolder callInvokerHolder = context.getCatalystInstance().getJSCallInvokerHolder();
    initializeNative(jsContext.get(), (CallInvokerHolderImpl) callInvokerHolder);
    return true;
  }

  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker);

//  @ReactMethod(isBlockingSynchronousMethod = true)
//  public void createSurfaceContext(Integer contextId) {
//    waitForNativeSurface(contextId);
//
//    ReactApplicationContext context = getReactApplicationContext();
//    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
//    createSurfaceContext(jsContext.get(), contextId);
//  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean createSurfaceContext(double contextId) {
    int a = (int)contextId;
    waitForNativeSurface(a);

    ReactApplicationContext context = getReactApplicationContext();
    JavaScriptContextHolder jsContext = context.getJavaScriptContextHolder();
    createSurfaceContext(jsContext.get(), a);
    return true;
  }

  @Override
  public void createSurfaceContextAsync(double contextId, Promise promise) {

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
