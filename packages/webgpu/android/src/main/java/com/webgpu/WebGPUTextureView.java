package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

@SuppressLint("ViewConstructor")
public class WebGPUTextureView extends TextureView implements TextureView.SurfaceTextureListener {

  WebGPUAPI mApi;
  private Surface mSurface;
  // A canvas keeps its picture when it is moved, as it does on the web and on
  // iOS (where the CAMetalLayer moves with its view). Android moves a view by
  // detaching it from its window and attaching it again, which is also how
  // React Native applies a change of zIndex, and a TextureView releases its
  // SurfaceTexture on detach. So the SurfaceTexture a detach releases is kept,
  // with the last frame it latched, and the native side keeps its surface too
  // (WebGPUAPI.surfaceParked); on reattach both are handed back, and the
  // canvas shows that frame without being drawn again. mDetaching marks the
  // release a detach causes: View calls onDetachedFromWindow() just before
  // TextureView releases the SurfaceTexture (onDetachedFromWindowInternal).
  private boolean mDetaching = false;
  @Nullable
  private SurfaceTexture mKeptSurfaceTexture = null;
  // Set by release(): the view is gone for good and keeps nothing.
  private boolean mReleased = false;
  // The kept SurfaceTexture was handed back and waits for this view to bind
  // it to its layer again before the canvas uses it (onSurfaceTextureUpdated).
  private boolean mUnparkOnBind = false;

  public WebGPUTextureView(Context context, WebGPUAPI api, boolean opaque) {
    super(context);
    mApi = api;
    setOpaque(opaque);
    setSurfaceTextureListener(this);
  }

  // Android 10 draws an opaque TextureView with SkBlendMode::kSrc, and under
  // a rotated, rounded clip its Skia (m75) can clear the stencil the clip was
  // written in before that draw, which leaves the canvas blank (fixed in
  // Skia m77, https://skia-review.googlesource.com/c/skia/+/223970). The
  // view blends on that release, whatever the GL: a canvas that covers its
  // bounds draws the same pixels.
  @Override
  public void setOpaque(boolean opaque) {
    super.setOpaque(opaque && Build.VERSION.SDK_INT != Build.VERSION_CODES.Q);
  }

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();
    mDetaching = false;
    SurfaceTexture kept = mKeptSurfaceTexture;
    mKeptSurfaceTexture = null;
    if (kept == null) {
      return;
    }
    // TextureView reports onSurfaceTextureAvailable() only for a
    // SurfaceTexture it creates itself, so the canvas takes its surface back
    // here. Its size may have changed while it was out of the window.
    setSurfaceTexture(kept);
    // TextureView binds a SurfaceTexture it is handed to its layer when it next
    // draws; until then nothing would consume the frames presented to it. So
    // the canvas stays parked until then (onSurfaceTextureUpdated).
    mApi.surfaceChanged(mSurface);
    mUnparkOnBind = true;
  }


  @Override
  protected void onDetachedFromWindow() {
    mUnparkOnBind = false;
    mDetaching = !mReleased;
    super.onDetachedFromWindow();
  }

  /**
   * The view is gone for good (WebGPUView.destroy(), after the native side has
   * let go of the surface): release what a detach kept, and keep nothing from
   * a detach still to come.
   */
  void release() {
    mReleased = true;
    mUnparkOnBind = false;
    if (mKeptSurfaceTexture != null) {
      if (mSurface != null) {
        mSurface.release();
        mSurface = null;
      }
      mKeptSurfaceTexture.release();
      mKeptSurfaceTexture = null;
    }
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mSurface = new Surface(surfaceTexture);
    mApi.surfaceCreated(mSurface);
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mApi.surfaceChanged(mSurface);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    if (mDetaching) {
      // Kept, with mSurface, for onAttachedToWindow(); release() frees them if
      // the view never comes back.
      mDetaching = false;
      mKeptSurfaceTexture = surfaceTexture;
      mApi.surfaceParked();
      return false;
    }
    // Detach first (synchronous through JNI) so the native side has dropped
    // its window reference before we release ours.
    mApi.surfaceOffscreen();
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
    return true;
  }

  /**
   * A frame was just presented from the main thread: have this draw show it.
   *
   * TextureView hears of a new frame through its SurfaceTexture's
   * frame-available listener, a plain message on the main looper. A frame
   * presented on the main thread during the Choreographer's animation
   * callbacks (the Reanimated UI runtime's requestAnimationFrame) is followed
   * by that frame's draw before the message runs: the draw shows nothing new,
   * the message schedules the next draw, and the canvas updates every other
   * frame (60 fps on a 120 Hz display). What the listener does is private
   * (updateLayerAndInvalidate()); onVisibilityChanged(VISIBLE) does the same
   * for a visible view, so this calls it before the draw. Main thread.
   */
  void latchPresentedFrame() {
    if (getSurfaceTexture() == null || !isShown()) {
      return;
    }
    super.onVisibilityChanged(this, View.VISIBLE);
  }

  /**
   * TextureView calls this from its draw after it latched a new frame, and
   * also right after it bound a SurfaceTexture it was handed to its layer
   * (getTextureLayer() forces that update). So a surface waiting for the bind
   * can be handed to the canvas now. Main thread.
   */
  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    if (!mUnparkOnBind || mReleased) {
      return;
    }
    mUnparkOnBind = false;
    if (!mApi.surfaceUnparked()) {
      // The native side gave the kept surface up while the view was away (its
      // device changed or was destroyed), and a new surface for the same
      // window could not connect. The canvas replaces this view with a new
      // one, which gets a new window the usual way. Posted: this draw is still
      // using the view.
      post(() -> mApi.backingViewLost(this));
    }
  }
}
