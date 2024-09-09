package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.view.Surface;
import android.view.TextureView;

import com.facebook.proguard.annotations.DoNotStrip;

public class WebGPUView extends TextureView {

  private Integer mContextId;

  private final SurfaceTexture mSurfaceTexture;
  private final Surface mSurface;
  private static boolean isDensitySet = false;

  @RequiresApi(api = Build.VERSION_CODES.O)
  public WebGPUView(Context context) {
    super(context);
    mSurfaceTexture = new SurfaceTexture(false);
    setSurfaceTexture(mSurfaceTexture);
    mSurface = new Surface(mSurfaceTexture);
    setSurfaceTextureListener(new SurfaceTextureListener() {
      @Override
      public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surface, int width, int height) {}

      @Override
      public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surface, int width, int height) {
        onSurfaceChanged(mSurface, mContextId, applyDensity(width), applyDensity(height));
      }

      @Override
      public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surface) {
        onSurfaceDestroy(mContextId);
        return false;
      }

      @Override
      public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surface) {}
    });

    if (!isDensitySet) {
      float density = getResources().getDisplayMetrics().density;
      setDensity(density);
      isDensitySet = true;
    }
  }

  public void setContextId(Integer contextId) {
    mContextId = contextId;
    float width = applyDensity(getWidth());
    float height = applyDensity(getHeight());
    onSurfaceCreate(mSurface, mContextId, width, height);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    androidSimulatorWorkaround();
  }

  float applyDensity(float size) {
    float density = getResources().getDisplayMetrics().density;
    return size / density;
  }

  void androidSimulatorWorkaround() {
    /*
      Software emulated TextureView on android emulator sometimes need additional call of
      invalidate method to flush gpu output
    */
    post(() -> {
      invalidate();
      postInvalidate();
    });
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
  private native void setDensity(float density);
}
