package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.hardware.HardwareBuffer;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A "normal RN view" backend for the WebGPU canvas.
 *
 * <p>WebGPU renders into a native pool of AHardwareBuffers (sized from the canvas drawing buffer,
 * like the swapchain) that Dawn imports as SharedTextureMemory; see SurfaceRegistry.h. Each finished
 * buffer is drawn inline here via {@link Bitmap#wrapHardwareBuffer} + {@link Canvas#drawBitmap}, so
 * this is a plain {@link View}: parent transforms, clipping, alpha, z-order and animations all
 * apply, with no GL interop and no extra copy.
 *
 * <p>This view is a pure consumer: it enables pool mode, and whenever the native fence waiter
 * signals that a frame is ready it asks native for the latest ready (gen, slot), wraps that buffer
 * in a cached Bitmap, and draws it scaled to the current bounds. Acquire is rigorous (native blocks
 * on Dawn's render-complete fence before a frame is ready); release is heuristic via a held-ring of
 * {@link #HELD_MAX} frames.
 */
@RequiresApi(api = Build.VERSION_CODES.Q)
@SuppressLint("ViewConstructor")
public class WebGPUHardwareBufferView extends View {

  private static final int HELD_MAX = 2; // displayed frames kept before release (release safety)

  private final WebGPUAPI mApi;
  private final Paint mPaint;
  private final Rect mSrc = new Rect();
  private final Rect mDst = new Rect();

  // token (generation << 32 | slot) -> wrapped hardware Bitmap (one per slot per generation).
  private final Map<Long, Bitmap> mBitmaps = new HashMap<>();
  // Held-ring of displayed frame tokens (newest last). The newest is what onDraw shows.
  private final ArrayDeque<Long> mHeld = new ArrayDeque<>();

  // Native -> UI thread wake-up. The fence waiter calls onNativeFrameReady() off the UI thread;
  // we coalesce those into at most one pending consume() on the main looper.
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private final AtomicBoolean mConsumePosted = new AtomicBoolean(false);
  private final Runnable mConsume = this::consume;

  private Bitmap mDisplayed;
  private int mDisplayedW;
  private int mDisplayedH;
  private int mCurrentGen;

  private boolean mAttached;
  private boolean mEnabled;

  public WebGPUHardwareBufferView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
    mPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    setWillNotDraw(false);
  }

  private int contextId() {
    return mApi.getContextId();
  }

  private static int genOf(long token) {
    return (int) (token >>> 32);
  }

  private static int slotOf(long token) {
    return (int) (token & 0xffffffffL);
  }

  // --- Sizing ----------------------------------------------------------------

  private void enablePool(int w, int h) {
    final float density = getResources().getDisplayMetrics().density;
    nEnablePool(contextId(), Math.max(1, Math.round(w / density)),
        Math.max(1, Math.round(h / density)));
    mEnabled = true;
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    super.onSizeChanged(w, h, oldw, oldh);
    if (w <= 0 || h <= 0) {
      return;
    }
    if (!mEnabled) {
      enablePool(w, h);
    } else {
      // Native reallocates the pool from the canvas drawing buffer; we only keep the dp
      // canvas-client size in sync so JS computes the right canvas.width/height.
      final float density = getResources().getDisplayMetrics().density;
      nSetClientSize(contextId(), Math.max(1, Math.round(w / density)),
          Math.max(1, Math.round(h / density)));
    }
  }

  // --- Consume (UI thread, woken by the native fence waiter) -----------------

  /** Called from the native fence-waiter thread (see cpp-adapter.cpp) when a frame is ready. */
  @Keep
  private void onNativeFrameReady() {
    if (mConsumePosted.compareAndSet(false, true)) {
      mMainHandler.post(mConsume);
    }
  }

  private void consume() {
    mConsumePosted.set(false);
    if (!mAttached) {
      return;
    }
    long token = nPollReady(contextId());
    if (token >= 0) {
      onFrameReady(token);
    }
  }

  private void onFrameReady(long token) {
    final int gen = genOf(token);
    final int slot = slotOf(token);

    if (gen > mCurrentGen) {
      mCurrentGen = gen;
      recycleStaleGenerations();
    }

    Bitmap bmp = mBitmaps.get(token);
    if (bmp == null) {
      HardwareBuffer hb = nGetHardwareBuffer(contextId(), gen, slot);
      if (hb == null) {
        return; // generation already retired; drop this stale frame
      }
      // wrapHardwareBuffer takes its own ref, so the wrapper can be closed immediately.
      bmp = Bitmap.wrapHardwareBuffer(hb, null);
      hb.close();
      if (bmp == null) {
        // Cannot display this frame; hand the slot straight back so the pool
        // does not drain (native marked it Displayed when we polled it).
        nReleaseSlot(contextId(), gen, slot);
        return;
      }
      mBitmaps.put(token, bmp);
    }

    mDisplayed = bmp;
    mDisplayedW = bmp.getWidth();
    mDisplayedH = bmp.getHeight();

    mHeld.addLast(token);
    while (mHeld.size() > HELD_MAX) {
      long old = mHeld.pollFirst();
      if (genOf(old) == mCurrentGen) {
        // Live generation reuses slots: hand it back once HWUI has had time to sample it.
        nReleaseSlot(contextId(), mCurrentGen, slotOf(old));
      } else {
        // Old generation: its slot is never re-rendered, so just drop the Bitmap.
        recycleToken(old);
      }
    }
    invalidate();
  }

  private void recycleStaleGenerations() {
    // Keep the current and previous generation (cross-fade); recycle older cached Bitmaps that are
    // no longer held.
    Iterator<Map.Entry<Long, Bitmap>> it = mBitmaps.entrySet().iterator();
    while (it.hasNext()) {
      Map.Entry<Long, Bitmap> e = it.next();
      if (genOf(e.getKey()) < mCurrentGen - 1 && !mHeld.contains(e.getKey())) {
        Bitmap b = e.getValue();
        if (b != mDisplayed && !b.isRecycled()) {
          b.recycle();
        }
        it.remove();
      }
    }
  }

  private void recycleToken(long token) {
    Bitmap b = mBitmaps.remove(token);
    if (b != null && b != mDisplayed && !b.isRecycled()) {
      b.recycle();
    }
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    Bitmap bmp = mDisplayed;
    if (bmp == null || bmp.isRecycled()) {
      return;
    }
    // Always scale the last good frame to the current bounds, so a buffer that lags the view size
    // (mid-resize) is shown stretched rather than blank.
    mSrc.set(0, 0, mDisplayedW, mDisplayedH);
    mDst.set(0, 0, getWidth(), getHeight());
    canvas.drawBitmap(bmp, mSrc, mDst, mPaint);
  }

  // --- Lifecycle -------------------------------------------------------------

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();
    mAttached = true;
    if (!mEnabled && getWidth() > 0 && getHeight() > 0) {
      enablePool(getWidth(), getHeight());
    }
    // Pick up anything that became ready while we were detached.
    onNativeFrameReady();
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    mAttached = false;
    mEnabled = false;
    mMainHandler.removeCallbacks(mConsume);
    mConsumePosted.set(false);

    // Keep the canvas alive offscreen, then drop all GPU resources. The
    // SurfaceInfo stays registered so a transient detach/re-attach keeps
    // working; WebGPUViewManager.onDropViewInstance removes it when RN drops
    // the view for good.
    nSwitchToOffscreen(contextId());

    mDisplayed = null;
    mHeld.clear();
    for (Bitmap b : mBitmaps.values()) {
      if (!b.isRecycled()) {
        b.recycle();
      }
    }
    mBitmaps.clear();
    mCurrentGen = 0;
  }

  @Override
  protected void onVisibilityChanged(@NonNull View changedView, int visibility) {
    super.onVisibilityChanged(changedView, visibility);
    if (visibility == VISIBLE && mAttached && !mEnabled
        && getWidth() > 0 && getHeight() > 0) {
      enablePool(getWidth(), getHeight());
    }
  }

  // --- Native (cpp-adapter.cpp) ----------------------------------------------

  private native void nEnablePool(int contextId, int dpW, int dpH);

  private native void nSetClientSize(int contextId, int dpW, int dpH);

  private native long nPollReady(int contextId);

  private native HardwareBuffer nGetHardwareBuffer(int contextId, int generation, int slot);

  private native void nReleaseSlot(int contextId, int generation, int slot);

  private native void nSwitchToOffscreen(int contextId);
}
