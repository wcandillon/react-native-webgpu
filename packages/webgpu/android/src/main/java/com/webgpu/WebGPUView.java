package com.webgpu;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;

public class WebGPUView extends TextureView implements TextureView.SurfaceTextureListener {

  private Integer mContextId;
  private WebGPUModule mModule;

  public WebGPUView(Context context) {
    super(context);
    setSurfaceTextureListener(this);
    setOpaque(false);
  }

  public void setContextId(Integer contextId) {
    if (mModule == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        mModule = ((ThemedReactContext) context).getReactApplicationContext().getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    float density = getResources().getDisplayMetrics().density;
    float scaledWidth = width / density;
    float scaledHeight = height / density;
    Surface surface = new Surface(surfaceTexture);
    onSurfaceCreate(surface, mContextId, scaledWidth, scaledHeight);
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    float density = getResources().getDisplayMetrics().density;
    float scaledWidth = width / density;
    float scaledHeight = height / density;
    Surface surface = new Surface(surfaceTexture);
    onSurfaceChanged(surface, mContextId, scaledWidth, scaledHeight);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    onSurfaceDestroy(mContextId);
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // This method can be used if you need to handle updates to the texture content.
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
