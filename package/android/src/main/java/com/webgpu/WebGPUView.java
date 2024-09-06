package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;

import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;
import com.facebook.proguard.annotations.DoNotStrip;

public class WebGPUView extends TextureView implements View.OnClickListener {

  private Integer mContextId;
  private WebGPUModule mModule;
//  private final WebGPUSurface mWebGPUSurface;

  private final SurfaceTexture mSurfaceTexture;
  private final Surface mSurface;

  @RequiresApi(api = Build.VERSION_CODES.O)
  public WebGPUView(Context context) {
    super(context);
    setOnClickListener(this::onClick);
//    mWebGPUSurface = new WebGPUSurface(context);
    mSurfaceTexture = new SurfaceTexture(false);
    setSurfaceTexture(mSurfaceTexture);
    mSurface = new Surface(mSurfaceTexture);
//    mSurfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
//      @Override
//      public void onFrameAvailable(SurfaceTexture surfaceTexture) {
//        Log.e("q", "q");
//      }
//    });
//    setSurfaceTextureListener(new SurfaceTextureListener() {
//      @Override
//      public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surface, int width, int height) {
//        Log.e("q", "q");
//      }
//
//      @Override
//      public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surface, int width, int height) {
//        Log.e("q", "q");
////        invalidate();
//      }
//
//      @Override
//      public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surface) {
//        return false;
//      }
//
//      @Override
//      public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surface) {
//        Log.e("q", "q");
//        invalidate();
////        postInvalidate();
//      }
//    });
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

  @Override
  public void onClick(View v) {
//    getDrawingTime();
    invalidate();
  }
}
