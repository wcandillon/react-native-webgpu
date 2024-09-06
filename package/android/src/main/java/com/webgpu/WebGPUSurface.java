package com.webgpu;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.view.Surface;
import android.view.TextureView;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

public class WebGPUSurface extends TextureView {

  private final SurfaceTexture mSurfaceTexture;
  private final Surface mSurface;

  @RequiresApi(api = Build.VERSION_CODES.O)
  public WebGPUSurface(@NonNull Context context) {
    super(context);
    mSurfaceTexture = new SurfaceTexture(true);
    setSurfaceTexture(mSurfaceTexture);
    mSurface = new Surface(mSurfaceTexture);
  }

  public Surface getSurface() {
    return mSurface;
  }

  public SurfaceTexture getSurfaceTexture() {
    return mSurfaceTexture;
  }
}
