package com.webgpu;

import android.graphics.HardwareBufferRenderer;
import android.graphics.RecordingCanvas;
import android.graphics.RenderNode;
import android.hardware.HardwareBuffer;
import android.hardware.SyncFence;
import android.os.Build;
import android.view.View;

import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;

import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.UIManager;
import com.facebook.react.bridge.UiThreadUtil;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.fabric.FabricUIManager;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.modules.blob.BlobModule;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.common.UIManagerType;

@ReactModule(name = WebGPUModule.NAME)
public class WebGPUModule extends NativeWebGPUModuleSpec {
  static {
      System.loadLibrary("react-native-webgpu"); // Load the C++ library
  }

  // Monotonic token handed to JS to retrieve a capture via
  // RNWebGPU.consumeCapturedElement(token).
  private static final AtomicInteger sCaptureToken = new AtomicInteger(0);

  // Off-loads the (blocking) wait on the render fence so it never runs on the
  // UI or RenderThread.
  private final ExecutorService mCaptureExecutor =
    Executors.newCachedThreadPool();

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
    return true;
  }

  // "HTML in Canvas": render the native view with the given React tag off-screen
  // into an AHardwareBuffer, then resolve with a token that JS exchanges for the
  // buffer handle via RNWebGPU.consumeCapturedElement(token).
  @ReactMethod
  @Override
  public void captureElement(double tag, Promise promise) {
    final int reactTag = (int) tag;
    final ReactApplicationContext context = getReactApplicationContext();
    UiThreadUtil.runOnUiThread(() -> {
      try {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
          promise.reject(
            "E_UNSUPPORTED",
            "copyElementImageToTexture requires Android 14 (API 34) or newer");
          return;
        }
        UIManager uiManager =
          UIManagerHelper.getUIManager(context, UIManagerType.FABRIC);
        if (!(uiManager instanceof FabricUIManager)) {
          promise.reject("E_NO_UIMANAGER", "Fabric UIManager not available");
          return;
        }
        View view = ((FabricUIManager) uiManager).resolveView(reactTag);
        if (view == null) {
          promise.reject("E_NO_VIEW", "No view found for tag " + reactTag);
          return;
        }
        int width = view.getWidth();
        int height = view.getHeight();
        if (width <= 0 || height <= 0) {
          promise.reject(
            "E_EMPTY_VIEW", "View " + reactTag + " has no size to capture yet");
          return;
        }
        renderViewToBuffer(view, width, height, promise);
      } catch (Throwable t) {
        promise.reject("E_CAPTURE", t);
      }
    });
  }

  @RequiresApi(api = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  private void renderViewToBuffer(View view, int width, int height, Promise promise) {
    final int token = sCaptureToken.incrementAndGet();

    // GPU-sampled (Dawn samples/copies it) + color output (HWUI renders into it).
    final HardwareBuffer buffer = HardwareBuffer.create(
      width, height, HardwareBuffer.RGBA_8888, 1,
      HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE
        | HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);

    // Record the view's draw commands into a RenderNode (must be on the UI
    // thread, where we already are).
    RenderNode node = new RenderNode("rnwgpu-element");
    node.setPosition(0, 0, width, height);
    RecordingCanvas canvas = node.beginRecording();
    view.draw(canvas);
    node.endRecording();

    final HardwareBufferRenderer renderer = new HardwareBufferRenderer(buffer);
    renderer.setContentRoot(node);

    Executor executor = mCaptureExecutor;
    renderer.obtainRenderRequest().draw(executor, (result) -> {
      try {
        if (result.getStatus() != HardwareBufferRenderer.RenderResult.SUCCESS) {
          promise.reject(
            "E_RENDER", "HardwareBufferRenderer status " + result.getStatus());
          return;
        }
        // v1 has no consumer-side wait fence, so block here until the GPU
        // render completes and the buffer holds final pixels.
        SyncFence fence = result.getFence();
        if (fence != null && fence.isValid()) {
          fence.awaitForever();
          fence.close();
        }
        // C++ acquires its own reference to the AHardwareBuffer.
        nativeStoreCapturedElement(token, buffer, width, height);
        promise.resolve((double) token);
      } catch (Throwable t) {
        promise.reject("E_RENDER", t);
      } finally {
        renderer.close();
        // Drop our Java reference; the buffer stays alive via the reference C++
        // took in nativeStoreCapturedElement (released by
        // RNWebGPU.releaseCapturedElement once the import has its own).
        buffer.close();
      }
    });
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker, BlobModule blobModule);

  @DoNotStrip
  private native void nativeStoreCapturedElement(int token, HardwareBuffer buffer, int width, int height);
}
