package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorSpace;
import android.graphics.Paint;
import android.graphics.Rect;
import android.hardware.HardwareBuffer;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.events.Event;
import com.facebook.react.uimanager.events.EventDispatcher;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * NativeBufferView: displays a native buffer that JS presented
 * (RNWebGPU.presentNativeBuffer), with no swap chain and no GPU device on the display side.
 *
 * <p>The contract: a buffer is never written again while it is displayed (the producer renders the
 * next content into another buffer), and this view never allocates one. It reports its size in
 * physical pixels (onPixelSizeChange) and draws whatever it is handed, scaled to its bounds by
 * {@code resizeMode}, until the producer catches up with a buffer of the new size.
 *
 * <p>Native code waits for the buffer's fence off the UI thread (NativeBufferViewRegistry) and then
 * wakes this view; it shows the buffer on the next vsync. API 29 and above: the buffer is drawn
 * inline, zero-copy, through {@link Bitmap#wrapHardwareBuffer}, which takes its own reference on
 * it. API 26 to 28: the pixels are read back into a regular Bitmap, and the buffer is free right
 * after the copy.
 */
@SuppressLint("ViewConstructor")
public class WebGPUNativeBufferView extends View {

  private static final String TAG = "WebGPUNativeBufferView";

  static final String PIXEL_SIZE_CHANGE_EVENT = "topPixelSizeChange";

  // NativeBufferContent metadata written by nTakeContent.
  private static final int INFO_SEQUENCE = 0;
  private static final int INFO_WIDTH = 1;
  private static final int INFO_HEIGHT = 2;
  private static final int INFO_HALF_FLOAT = 3;
  private static final int INFO_DISPLAY_P3 = 4;
  private static final int INFO_OPAQUE = 5;
  private static final int INFO_SIZE = 6;

  private enum ResizeMode { STRETCH, CONTAIN, COVER, TOP_LEFT }

  private final Paint mPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
  private final Rect mSrc = new Rect();
  private final Rect mDst = new Rect();
  private final long[] mInfo = new long[INFO_SIZE];

  // Native -> UI thread wake-up, coalesced into at most one pending consume().
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private final AtomicBoolean mConsumePosted = new AtomicBoolean(false);
  private final Runnable mConsume = this::consume;

  private int mViewId;
  private long mSequence;
  private ResizeMode mResizeMode = ResizeMode.STRETCH;
  @Nullable private Bitmap mBitmap;
  private int mReportedWidth;
  private int mReportedHeight;

  public WebGPUNativeBufferView(Context context) {
    super(context);
    setWillNotDraw(false);
  }

  // --- Props -------------------------------------------------------------------

  public void setViewId(int viewId) {
    if (viewId == mViewId) {
      return;
    }
    if (mViewId != 0) {
      nDetach(mViewId);
    }
    mViewId = viewId;
    mSequence = 0;
    if (viewId != 0) {
      // Calls onNativeContentReady right away when content is already there.
      nAttach(viewId);
    }
  }

  public void setResizeMode(@Nullable String value) {
    ResizeMode mode;
    if ("contain".equals(value)) {
      mode = ResizeMode.CONTAIN;
    } else if ("cover".equals(value)) {
      mode = ResizeMode.COVER;
    } else if ("topLeft".equals(value)) {
      mode = ResizeMode.TOP_LEFT;
    } else {
      mode = ResizeMode.STRETCH;
    }
    if (mode != mResizeMode) {
      mResizeMode = mode;
      invalidate();
    }
  }

  // --- Size --------------------------------------------------------------------

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    super.onSizeChanged(w, h, oldw, oldh);
    // The size the producer should render at for a 1:1 display. Layout has
    // already rounded it to whole pixels; JS gets exactly these numbers.
    if (w > 0 && h > 0 && (w != mReportedWidth || h != mReportedHeight)) {
      mReportedWidth = w;
      mReportedHeight = h;
      dispatchPixelSize(w, h);
    }
  }

  private void dispatchPixelSize(int width, int height) {
    Context context = getContext();
    if (!(context instanceof ReactContext)) {
      return;
    }
    ReactContext reactContext = (ReactContext) context;
    EventDispatcher dispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, getId());
    if (dispatcher != null) {
      dispatcher.dispatchEvent(
          new PixelSizeChangeEvent(UIManagerHelper.getSurfaceId(this), getId(), width, height));
    }
  }

  // --- Content (UI thread, woken by native) ---------------------------------------

  /** Called by native, from any thread, when new content is ready. */
  @Keep
  private void onNativeContentReady() {
    if (mConsumePosted.compareAndSet(false, true)) {
      mMainHandler.post(mConsume);
    }
  }

  private void consume() {
    mConsumePosted.set(false);
    if (mViewId == 0) {
      return;
    }
    HardwareBuffer buffer = nTakeContent(mViewId, mSequence, mInfo);
    if (buffer == null) {
      return;
    }
    mSequence = mInfo[INFO_SEQUENCE];
    Bitmap bitmap;
    try {
      bitmap = toBitmap(buffer);
    } finally {
      // The Bitmap holds its own reference (or a copy below API 29).
      buffer.close();
    }
    if (bitmap == null) {
      return;
    }
    Bitmap previous = mBitmap;
    mBitmap = bitmap;
    invalidate();
    // The display list recorded with the previous bitmap keeps its own native
    // reference until it is replaced; this only drops ours.
    if (previous != null && !previous.isRecycled()) {
      previous.recycle();
    }
  }

  @Nullable
  private Bitmap toBitmap(HardwareBuffer buffer) {
    final boolean halfFloat = mInfo[INFO_HALF_FLOAT] != 0;
    final ColorSpace colorSpace = colorSpaceFor(halfFloat, mInfo[INFO_DISPLAY_P3] != 0);
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      return wrap(buffer, colorSpace);
    }
    return readBack(buffer, halfFloat, colorSpace);
  }

  @RequiresApi(api = Build.VERSION_CODES.Q)
  @Nullable
  private static Bitmap wrap(HardwareBuffer buffer, ColorSpace colorSpace) {
    try {
      Bitmap bitmap = Bitmap.wrapHardwareBuffer(buffer, colorSpace);
      if (bitmap == null) {
        Log.e(TAG, "Bitmap.wrapHardwareBuffer failed: the buffer needs GPU_SAMPLED_IMAGE usage");
      }
      return bitmap;
    } catch (IllegalArgumentException e) {
      Log.e(TAG, "Bitmap.wrapHardwareBuffer failed", e);
      return null;
    }
  }

  // Android 8 and 9: no wrapHardwareBuffer. The pixels are copied into a
  // regular Bitmap (the buffer needs CPU_READ_RARELY usage, which
  // createNativeBuffer adds there); the copy is free to go right after.
  @Nullable
  private Bitmap readBack(HardwareBuffer buffer, boolean halfFloat, ColorSpace colorSpace) {
    final int width = (int) mInfo[INFO_WIDTH];
    final int height = (int) mInfo[INFO_HEIGHT];
    final boolean hasAlpha = mInfo[INFO_OPAQUE] == 0;
    final Bitmap.Config config = halfFloat ? Bitmap.Config.RGBA_F16 : Bitmap.Config.ARGB_8888;
    Bitmap bitmap;
    try {
      bitmap = Bitmap.createBitmap(width, height, config, hasAlpha, colorSpace);
    } catch (IllegalArgumentException e) {
      // Android 8 and 9 only take a linear color space for RGBA_F16 bitmaps.
      Log.w(TAG, "no " + colorSpace.getName() + " bitmap for this format; colors may be off");
      bitmap = Bitmap.createBitmap(width, height, config);
    }
    if (!nReadPixels(buffer, bitmap)) {
      Log.e(TAG, "reading the buffer back failed (it needs CPU_READ_RARELY usage below API 29)");
      bitmap.recycle();
      return null;
    }
    return bitmap;
  }

  private static ColorSpace colorSpaceFor(boolean halfFloat, boolean displayP3) {
    if (displayP3) {
      return ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
    }
    return ColorSpace.get(halfFloat ? ColorSpace.Named.EXTENDED_SRGB : ColorSpace.Named.SRGB);
  }

  // --- Draw --------------------------------------------------------------------

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    Bitmap bitmap = mBitmap;
    if (bitmap == null || bitmap.isRecycled()) {
      return;
    }
    final int bw = bitmap.getWidth();
    final int bh = bitmap.getHeight();
    final int vw = getWidth();
    final int vh = getHeight();
    if (bw <= 0 || bh <= 0 || vw <= 0 || vh <= 0) {
      return;
    }
    // A buffer that lags the view size (mid-resize) is scaled, never blank.
    switch (mResizeMode) {
      case CONTAIN: {
        final float scale = Math.min((float) vw / bw, (float) vh / bh);
        final int dw = Math.round(bw * scale);
        final int dh = Math.round(bh * scale);
        mSrc.set(0, 0, bw, bh);
        mDst.set((vw - dw) / 2, (vh - dh) / 2, (vw - dw) / 2 + dw, (vh - dh) / 2 + dh);
        break;
      }
      case COVER: {
        // Crop the source to the view's aspect ratio rather than clipping.
        final float scale = Math.max((float) vw / bw, (float) vh / bh);
        final int sw = Math.min(bw, Math.round(vw / scale));
        final int sh = Math.min(bh, Math.round(vh / scale));
        mSrc.set((bw - sw) / 2, (bh - sh) / 2, (bw - sw) / 2 + sw, (bh - sh) / 2 + sh);
        mDst.set(0, 0, vw, vh);
        break;
      }
      case TOP_LEFT: {
        // No scaling: crop instead of blur.
        final int w = Math.min(bw, vw);
        final int h = Math.min(bh, vh);
        mSrc.set(0, 0, w, h);
        mDst.set(0, 0, w, h);
        break;
      }
      case STRETCH:
      default:
        mSrc.set(0, 0, bw, bh);
        mDst.set(0, 0, vw, vh);
        break;
    }
    if (bitmap.getConfig() == Bitmap.Config.HARDWARE && !canvas.isHardwareAccelerated()) {
      // A software canvas (a view capture with view.draw(new Canvas(bitmap)), a
      // software layer) throws on a hardware bitmap: draw a copy of the pixels.
      Bitmap copy = softwareCopy(bitmap);
      if (copy != null) {
        canvas.drawBitmap(copy, mSrc, mDst, mPaint);
        copy.recycle();
      }
      return;
    }
    canvas.drawBitmap(bitmap, mSrc, mDst, mPaint);
  }

  // Reads the buffer back; null when the pixels can't be read (the capture
  // then shows nothing for this view).
  @Nullable
  private static Bitmap softwareCopy(Bitmap hardware) {
    try {
      return hardware.copy(Bitmap.Config.ARGB_8888, false);
    } catch (RuntimeException e) {
      Log.w(TAG, "no software copy of the buffer for a software canvas", e);
      return null;
    }
  }

  // --- Lifecycle -----------------------------------------------------------------

  /** RN dropped the view for good (WebGPUNativeBufferViewManager.onDropViewInstance). */
  public void destroy() {
    if (mViewId != 0) {
      nDetach(mViewId);
      mViewId = 0;
    }
    mMainHandler.removeCallbacks(mConsume);
    mConsumePosted.set(false);
    if (mBitmap != null && !mBitmap.isRecycled()) {
      mBitmap.recycle();
    }
    mBitmap = null;
  }

  // --- Event -------------------------------------------------------------------

  private static class PixelSizeChangeEvent extends Event<PixelSizeChangeEvent> {
    private final int mWidth;
    private final int mHeight;

    PixelSizeChangeEvent(int surfaceId, int viewTag, int width, int height) {
      super(surfaceId, viewTag);
      mWidth = width;
      mHeight = height;
    }

    @Override
    public String getEventName() {
      return PIXEL_SIZE_CHANGE_EVENT;
    }

    @Override
    protected WritableMap getEventData() {
      WritableMap data = Arguments.createMap();
      data.putInt("width", mWidth);
      data.putInt("height", mHeight);
      return data;
    }
  }

  // --- Native (cpp-adapter.cpp) ------------------------------------------------

  private native void nAttach(int viewId);

  private static native void nDetach(int viewId);

  // The newest content after `afterSequence` as a HardwareBuffer the caller
  // closes, or null; its metadata goes to `info` (see INFO_*).
  private static native HardwareBuffer nTakeContent(int viewId, long afterSequence, long[] info);

  private static native boolean nReadPixels(HardwareBuffer buffer, Bitmap bitmap);
}
