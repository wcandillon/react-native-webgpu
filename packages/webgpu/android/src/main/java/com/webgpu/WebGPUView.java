package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.view.Choreographer;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {


  private int mContextId;
  private boolean mTransparent = false;
  private WebGPUModule mModule;
  private View mView = null;
  private boolean mPresentLoopRunning = false;
  private final Choreographer.FrameCallback mPresentCallback = new Choreographer.FrameCallback() {
    @Override
    public void doFrame(long frameTimeNanos) {
      if (!mPresentLoopRunning) {
        return;
      }
      nativePresent(mContextId);
      Choreographer.getInstance().postFrameCallback(this);
    }
  };

  WebGPUView(Context context) {
    super(context);
  }

  private void startPresentLoop() {
    if (mPresentLoopRunning) {
      return;
    }
    mPresentLoopRunning = true;
    Choreographer.getInstance().postFrameCallback(mPresentCallback);
  }

  private void stopPresentLoop() {
    if (!mPresentLoopRunning) {
      return;
    }
    mPresentLoopRunning = false;
    Choreographer.getInstance().removeFrameCallback(mPresentCallback);
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

  public void setTransparent(boolean value) {
    Context ctx = getContext();
    if (value != mTransparent || mView == null) {
      if (mView != null) {
        removeView(mView);
      }
      mTransparent = value;
      if (mTransparent) {
//        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
//          mView = new WebGPUAHBView(ctx, this);
//        } else {
          mView = new WebGPUTextureView(ctx, this);
//        }
      } else {
        mView = new WebGPUSurfaceView(ctx, this);
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
    startPresentLoop();
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
    stopPresentLoop();
    onSurfaceDestroy(mContextId);
  }

  @Override
  public void surfaceOffscreen() {
    stopPresentLoop();
    switchToOffscreenSurface(mContextId);
  }

  @Override
  protected void onDetachedFromWindow() {
    stopPresentLoop();
    super.onDetachedFromWindow();
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

  @DoNotStrip
  private native void nativePresent(int contextId);

}
