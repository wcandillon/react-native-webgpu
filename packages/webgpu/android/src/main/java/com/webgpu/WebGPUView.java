package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.view.Surface;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.StringDef;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {

  public static final String SURFACE_VIEW = "SurfaceView";
  public static final String TEXTURE_VIEW = "TextureView";
  public static final String HARDWARE_BUFFER = "HardwareBuffer";
  public static final String SURFACE_VIEW2 = "SurfaceView2";

  @Retention(RetentionPolicy.SOURCE)
  @StringDef({
    SURFACE_VIEW,
    TEXTURE_VIEW,
    HARDWARE_BUFFER,
    SURFACE_VIEW2
  })
  public @interface ViewType {}

  private int mContextId;
  private @ViewType String mName = null;
  private WebGPUModule mModule;
  private View mView;

  WebGPUView(Context context) {
    super(context);
  }

  public void setContextId(int contextId) {
    if (mModule == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        mModule = ((ThemedReactContext) context).getReactApplicationContext().getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
  }

  public void setView(@NonNull @ViewType String name) {
    Context ctx = getContext();
    if (mName == null || !mName.equals(name)) {
      removeView(mView);
      mName = name;
      switch (name) {
        case TEXTURE_VIEW -> mView = new WebGPUTextureView(ctx, this);
        case HARDWARE_BUFFER -> {
          if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mView = new WebGPUAHBView(ctx, this);
          } else {
            throw new RuntimeException("HardwareBuffer Canvas implementation is only available on API Level 29 and above");
          }
        }
        case SURFACE_VIEW2 -> {
          if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mView = new SurfaceView2(ctx, this);
          } else {
            throw new RuntimeException("HardwareBuffer Canvas implementation is only available on API Level 29 and above");
          }
        }
        default -> mView = new WebGPUSurfaceView(ctx, this);
      }
      addView(mView);
    }
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    mView.layout(0, 0, this.getMeasuredWidth(), this.getMeasuredHeight());
  }

  @Override
  public void surfaceCreated(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    onSurfaceCreate(surface, mContextId, width, height);
  }

  @Override
  public void surfaceChanged(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    onSurfaceChanged(surface, mContextId, width, height);
  }

  @Override
  public void surfaceDestroyed() {
    onSurfaceDestroy(mContextId);
  }

  @Override
  public void surfaceOffscreen() {
    switchToOffscreenSurface(mContextId);
  }

  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float width,
    float height
  );

  @DoNotStrip
  private native void onSurfaceChanged(
    Surface surface,
    int contextId,
    float width,
    float height
  );

  @DoNotStrip
  private native void onSurfaceDestroy(int contextId);

  @DoNotStrip
  private native void switchToOffscreenSurface(int contextId);

}
