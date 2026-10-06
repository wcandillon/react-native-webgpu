package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import android.view.TextureView;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Presents the WebGPU canvas with a copy.
 *
 * <p>WebGPU renders into textures it owns and, on the UI thread, this view copies the latest
 * finished one onto its own surface and presents it (see BlitPresenter.h). The surface is never
 * used from the rendering thread. This is what a {@link WebGPUHardwareBufferView} is replaced with
 * when hardware buffers cannot back the canvas, which is why it is a TextureView: real view
 * content that composites with alpha.
 *
 * <p>Frames are paced by the consumer. After presenting one, the next waits until HWUI has latched
 * it ({@link #onSurfaceTextureUpdated}). The buffer queue only drains when this view is drawn, and
 * that happens on this same thread, so presenting ahead of it could block the UI thread waiting
 * for a buffer that only the UI thread can free. A frame that finishes in the meantime replaces
 * the one waiting: native keeps only the latest.
 */
@SuppressLint("ViewConstructor")
public class WebGPUBlitTextureView extends TextureView
    implements TextureView.SurfaceTextureListener {

  private final WebGPUAPI mApi;
  private Surface mSurface;

  // Native -> UI thread wake-up. The rendering thread calls onNativeFrameReady(); those calls are
  // coalesced into at most one pending present() on the main looper.
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private final AtomicBoolean mPresentPosted = new AtomicBoolean(false);
  private final Runnable mPresent = this::present;

  // A buffer was queued and HWUI has not latched it yet.
  private boolean mAwaitingLatch;

  public WebGPUBlitTextureView(Context context, WebGPUAPI api, boolean opaque) {
    super(context);
    mApi = api;
    setOpaque(opaque);
    setSurfaceTextureListener(this);
  }

  private int contextId() {
    return mApi.getContextId();
  }

  private int toDp(int px) {
    final float density = getResources().getDisplayMetrics().density;
    return Math.max(1, Math.round(px / density));
  }

  // --- Present (UI thread, woken by the rendering thread) --------------------

  /** Called from the rendering thread (see cpp-adapter.cpp) when a frame is finished. */
  @Keep
  private void onNativeFrameReady() {
    schedulePresent();
  }

  private void schedulePresent() {
    if (mPresentPosted.compareAndSet(false, true)) {
      mMainHandler.post(mPresent);
    }
  }

  private void present() {
    mPresentPosted.set(false);
    if (mSurface == null || mAwaitingLatch) {
      return;
    }
    if (nPresentFrame(contextId())) {
      mAwaitingLatch = true;
    }
  }

  // --- Surface lifecycle ------------------------------------------------------

  @Override
  public void onSurfaceTextureAvailable(
      @NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mSurface = new Surface(surfaceTexture);
    mAwaitingLatch = false;
    nAttach(mSurface, contextId(), toDp(width), toDp(height));
    // Show the latest frame right away, if there is one.
    present();
  }

  @Override
  public void onSurfaceTextureSizeChanged(
      @NonNull SurfaceTexture surfaceTexture, int width, int height) {
    // Native sizes the frames from the canvas drawing buffer; we only keep the dp canvas-client
    // size in sync so JS computes the right canvas.width/height.
    nSetClientSize(contextId(), toDp(width), toDp(height));
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    mMainHandler.removeCallbacks(mPresent);
    mPresentPosted.set(false);
    mAwaitingLatch = false;
    // Detach first (synchronous through JNI) so the native side has dropped its window reference
    // before we release ours.
    nDetach(contextId());
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // HWUI latches the buffer during this draw pass. Present the next frame once the pass is over
    // rather than from inside it: by then the previous buffer is back in the queue.
    mAwaitingLatch = false;
    schedulePresent();
  }

  // --- Native (cpp-adapter.cpp) ----------------------------------------------

  private native void nAttach(Surface surface, int contextId, int dpW, int dpH);

  private native void nSetClientSize(int contextId, int dpW, int dpH);

  private native boolean nPresentFrame(int contextId);

  private native void nDetach(int contextId);
}
