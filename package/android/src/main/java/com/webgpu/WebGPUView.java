package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;

import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.proguard.annotations.DoNotStrip;

public class WebGPUView extends TextureView {

  private Integer mContextId;
  private WebGPUModule mModule;

  private final SurfaceTexture mSurfaceTexture;
  private final Surface mSurface;

  @RequiresApi(api = Build.VERSION_CODES.O)
  public WebGPUView(Context context) {
    super(context);
    mSurfaceTexture = new SurfaceTexture(false);
    setSurfaceTexture(mSurfaceTexture);
    mSurface = new Surface(mSurfaceTexture);
    setSurfaceTextureListener(new SurfaceTextureListener() {
      @Override
      public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surface, int width, int height) {
        Log.e("q", "q1");
      }

      @Override
      public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surface, int width, int height) {
        Log.e("q", "q2");
      }

      @Override
      public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surface) {
        return false;
      }

      @Override
      public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surface) {
        Log.e("q", "q3");
      }
    });
  }

  public void setContextId(Integer contextId) {
    if (mModule == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        mModule = ((ThemedReactContext) context).getReactApplicationContext().getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
    onSurfaceCreate(mSurface, mContextId, getWidth(), getHeight());
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    androidSimulatorWorkaround();
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
}
