package com.webgpu;

import android.graphics.Bitmap;
import android.util.Log;
import android.view.View;

import androidx.annotation.OptIn;

import java.util.HashSet;
import java.util.Set;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.UIManager;
import com.facebook.react.bridge.UiThreadUtil;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.modules.blob.BlobModule;
import com.facebook.react.modules.blob.BlobProvider;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;
import com.facebook.react.turbomodule.core.interfaces.CallInvokerHolder;

@ReactModule(name = WebGPUModule.NAME)
public class WebGPUModule extends NativeWebGPUModuleSpec {
  static {
      System.loadLibrary("react-native-webgpu"); // Load the C++ library
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
    initializeNative(jsContext.get(), (CallInvokerHolderImpl) callInvokerHolder, blobModule);
    return true;
  }

  @OptIn(markerClass = FrameworkAPI.class)
  @DoNotStrip
  private native void initializeNative(long jsRuntime, CallInvokerHolderImpl jsInvoker, BlobModule blobModule);

  /**
   * Called from C++ (AndroidPlatformContext::snapshotView) on an arbitrary
   * thread for GPUQueue.drawElementImageToTexture. Hops to the UI thread,
   * resolves the view for {@code tag}, rasterizes the requested rectangle and
   * always reports back through {@link #onViewSnapshot}, with either a bitmap
   * or an error message. {@code callback} is an opaque native pointer that
   * must be handed back exactly once.
   *
   * The source rectangle is in density-independent points (React Native
   * layout units) and is converted to pixels here; a non-positive
   * sourceWidth/sourceHeight means the whole view. A zero width/height means
   * the natural pixel size of the source rectangle.
   */
  @DoNotStrip
  void snapshotView(
      final int tag,
      final double sourceX,
      final double sourceY,
      final double sourceWidth,
      final double sourceHeight,
      final int width,
      final int height,
      final long callback) {
    UiThreadUtil.runOnUiThread(
        () -> {
          Bitmap bitmap = null;
          String error = null;
          try {
            ReactApplicationContext context = getReactApplicationContext();
            UIManager uiManager = UIManagerHelper.getUIManagerForReactTag(context, tag);
            View view = uiManager == null ? null : uiManager.resolveView(tag);
            if (view == null) {
              throw new IllegalArgumentException(
                  "drawElementImageToTexture: no native view found for tag "
                      + tag
                      + " (is the view mounted, and rendered with collapsable={false}?)");
            }
            if (view.getWidth() <= 0 || view.getHeight() <= 0) {
              throw new IllegalStateException(
                  "drawElementImageToTexture: the view has no size yet");
            }
            float density = view.getResources().getDisplayMetrics().density;
            float sx = (float) (sourceX * density);
            float sy = (float) (sourceY * density);
            float sw = sourceWidth > 0 ? (float) (sourceWidth * density) : view.getWidth();
            float sh = sourceHeight > 0 ? (float) (sourceHeight * density) : view.getHeight();
            int w = width > 0 ? width : Math.round(sw);
            int h = height > 0 ? height : Math.round(sh);
            if (w <= 0 || h <= 0) {
              throw new IllegalArgumentException(
                  "drawElementImageToTexture: the source rectangle is empty");
            }
            bitmap = ViewSnapshot.snapshot(view, sx, sy, sw, sh, w, h);
          } catch (Throwable t) {
            error = t.getMessage() != null ? t.getMessage() : t.toString();
          }
          onViewSnapshot(callback, bitmap, error);
        });
  }

  @DoNotStrip
  private native void onViewSnapshot(long callback, Bitmap bitmap, String error);
}
