package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.graphics.Region;
import android.os.Build;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.concurrent.atomic.AtomicLong;

@SuppressLint("ViewConstructor")
public class WebGPUSurfaceView extends SurfaceView implements SurfaceHolder.Callback2 {

  // Unique across views: a first frame reported for a surface the canvas has
  // since replaced, in this view or the one before it, matches no live token.
  private static final AtomicLong sNextSurfaceToken = new AtomicLong();

  WebGPUAPI mApi;
  private final FirstFrameDrawGate mDrawGate = new FirstFrameDrawGate(new FirstFrameDrawGate.Host() {
    @Override
    public void postDelayed(Runnable action, long delayMillis) {
      WebGPUSurfaceView.this.postDelayed(action, delayMillis);
    }

    @Override
    public void removeCallbacks(Runnable action) {
      WebGPUSurfaceView.this.removeCallbacks(action);
    }

    @Override
    public void invalidate() {
      WebGPUSurfaceView.this.invalidate();
    }
  }, Build.VERSION.SDK_INT);

  public WebGPUSurfaceView(Context context, WebGPUAPI api, boolean zOrderOnTop,
      boolean zOrderMediaOverlay, boolean opaque) {
    super(context);
    mApi = api;
    // Both are read when the surface is created.
    setZOrder(zOrderOnTop, zOrderMediaOverlay);
    setOpaque(opaque);
    getHolder().addCallback(this);
  }

  // The format drives the compositor's opaque flag for this layer. It can
  // change on a live surface: SurfaceView reports it through surfaceChanged.
  public void setOpaque(boolean opaque) {
    getHolder().setFormat(opaque ? PixelFormat.OPAQUE : PixelFormat.TRANSLUCENT);
  }

  // The sublayer the surface is composited at, relative to the window: above
  // it (on top), or below it, above the other SurfaceViews there (media
  // overlay) or among them (the default). setZOrderOnTop(false) resets the
  // request to the default, so it goes first. Before Android 11 the request
  // is read when the surface is created; from Android 11 on, SurfaceView
  // applies a changed request with the window's next frame (its pre-draw
  // listener runs updateSurface()), which invalidate() schedules.
  public void setZOrder(boolean onTop, boolean mediaOverlay) {
    setZOrderOnTop(onTop);
    if (!onTop) {
      setZOrderMediaOverlay(mediaOverlay);
    }
    invalidate();
  }

  // SurfaceView skips draw() and punches its hole in dispatchDraw.
  @Override
  protected void dispatchDraw(@NonNull Canvas canvas) {
    if (mDrawGate.hasFrame()) {
      super.dispatchDraw(canvas);
    }
  }

  // SurfaceView adds its bounds to the window's transparent region once its
  // draw is reported, and the compositor skips the window there.
  @Override
  public boolean gatherTransparentRegion(@Nullable Region region) {
    return !mDrawGate.hasFrame() || super.gatherTransparentRegion(region);
  }

  @Override
  protected void onDetachedFromWindow() {
    mDrawGate.onDetachedFromWindow();
    super.onDetachedFromWindow();
    // surfaceDestroyed() normally fires during detach as well; going offscreen
    // is idempotent, so this is just a safety net for paths where it does not.
    releaseSurface();
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    long surfaceToken = sNextSurfaceToken.incrementAndGet();
    mDrawGate.onSurfaceCreated(surfaceToken);
    mApi.surfaceCreated(holder.getSurface(), surfaceToken);
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    mApi.surfaceChanged(holder.getSurface());
  }

  @Override
  public void surfaceRedrawNeeded(@NonNull SurfaceHolder holder) {
    // Answered in surfaceRedrawNeededAsync, which SurfaceView calls instead.
  }

  // The draw is reported finished once a frame is in the surface: see
  // FirstFrameDrawGate. GLSurfaceView holds this runnable the same way.
  @Override
  public void surfaceRedrawNeededAsync(@NonNull SurfaceHolder holder, @NonNull Runnable drawingFinished) {
    mDrawGate.onRedrawNeeded(drawingFinished);
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    releaseSurface();
  }

  /** A frame was presented into the surface surfaceToken names. Main thread. */
  void onFirstFramePresented(long surfaceToken) {
    mDrawGate.onFirstFramePresented(surfaceToken);
  }

  private void releaseSurface() {
    mDrawGate.onSurfaceReleased();
    mApi.surfaceOffscreen();
  }
}
