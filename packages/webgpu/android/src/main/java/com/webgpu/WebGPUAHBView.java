package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorSpace;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

@RequiresApi(api = Build.VERSION_CODES.Q)
@SuppressLint("ViewConstructor")
public class WebGPUAHBView extends View implements ImageReader.OnImageAvailableListener {

  private static final String TAG = "WebGPUAHBView";
  private static final int MAX_IMAGES = 3;
  private static final int IMAGE_USAGE =
    (int) (HardwareBuffer.USAGE_GPU_COLOR_OUTPUT |
          HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
          HardwareBuffer.USAGE_COMPOSER_OVERLAY);

  private final WebGPUAPI mApi;
  private final Paint mPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
  private final Object mBitmapLock = new Object();
  private final Rect mDstRect = new Rect();
  private final ColorSpace mColorSpace = ColorSpace.get(ColorSpace.Named.SRGB);

  private final HandlerThread mImageThread;
  private final Handler mImageHandler;

  private ImageReader mImageReader;
  private Surface mSurface;
  private Bitmap mLatestBitmap;
  private boolean mSurfaceReported = false;
  private int mBufferWidth = 0;
  private int mBufferHeight = 0;

  WebGPUAHBView(@NonNull Context context, @NonNull WebGPUAPI api) {
    super(context);
    mApi = api;
    setWillNotDraw(false);
    setLayerType(LAYER_TYPE_HARDWARE, null);

    mImageThread = new HandlerThread("WebGPU-AHB");
    mImageThread.start();
    mImageHandler = new Handler(mImageThread.getLooper());
  }

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();
    maybeConfigureImageReader(getWidth(), getHeight());
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    destroySurface();
    teardownImageReader();
    mImageThread.quitSafely();
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    super.onSizeChanged(w, h, oldw, oldh);
    maybeConfigureImageReader(w, h);
  }

  @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
  private void maybeConfigureImageReader(int width, int height) {
    if (width <= 0 || height <= 0) {
      return;
    }

    if (mImageReader != null && width == mBufferWidth && height == mBufferHeight) {
      return;
    }

    teardownImageReader();

    try {
      mImageReader = new ImageReader.Builder(width, height)
        .setUsage(IMAGE_USAGE)
        .setMaxImages(MAX_IMAGES)
        .build();
      mImageReader.setOnImageAvailableListener(this, mImageHandler);

      mSurface = mImageReader.getSurface();
      mBufferWidth = width;
      mBufferHeight = height;
      notifySurface();
    } catch (Exception e) {
      Log.e(TAG, "Unable to create ImageReader surface", e);
      mApi.surfaceOffscreen();
      teardownImageReader();
    }
  }

  private void notifySurface() {
    if (mSurface == null) {
      return;
    }
    if (!mSurfaceReported) {
      mApi.surfaceCreated(mSurface);
      mSurfaceReported = true;
    } else {
      mApi.surfaceChanged(mSurface);
    }
  }

  private void destroySurface() {
    if (mSurfaceReported) {
      mApi.surfaceDestroyed();
      mSurfaceReported = false;
    }
  }

  private void teardownImageReader() {
    synchronized (mBitmapLock) {
      mLatestBitmap = null;
    }

    if (mImageReader != null) {
      mImageReader.close();
      mImageReader = null;
    }

    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }

    mBufferWidth = 0;
    mBufferHeight = 0;
  }

  @Override
  public void onImageAvailable(ImageReader reader) {
    Image image = null;
    try {
      image = reader.acquireLatestImage();
      if (image == null || reader != mImageReader) {
        return;
      }

      HardwareBuffer buffer = image.getHardwareBuffer();
      if (buffer == null) {
        return;
      }

      Bitmap bitmap = null;
      try {
        bitmap = Bitmap.wrapHardwareBuffer(buffer, mColorSpace);
      } finally {
        buffer.close();
      }

      if (bitmap == null) {
        return;
      }

      synchronized (mBitmapLock) {
        mLatestBitmap = bitmap;
      }

      postInvalidateOnAnimation();
    } catch (Exception e) {
      Log.e(TAG, "Failed to present frame", e);
    } finally {
      if (image != null) {
        image.close();
      }
    }
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    Bitmap bitmap;
    synchronized (mBitmapLock) {
      bitmap = mLatestBitmap;
    }

    if (bitmap == null || bitmap.isRecycled()) {
      return;
    }

    int width = getWidth();
    int height = getHeight();
    if (width <= 0 || height <= 0) {
      return;
    }

    if (bitmap.getWidth() == width && bitmap.getHeight() == height) {
      canvas.drawBitmap(bitmap, 0, 0, mPaint);
    } else {
      mDstRect.set(0, 0, width, height);
      canvas.drawBitmap(bitmap, null, mDstRect, mPaint);
    }
  }

  @Override
  public void setAlpha(float alpha) {
    super.setAlpha(alpha);
    mPaint.setAlpha((int) (alpha * 255));
    invalidate();
  }
}
