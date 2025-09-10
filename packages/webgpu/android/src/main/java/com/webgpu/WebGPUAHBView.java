package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.nio.ByteBuffer;

@RequiresApi(api = Build.VERSION_CODES.Q)
@SuppressLint("ViewConstructor")
public class WebGPUAHBView extends View implements ImageReader.OnImageAvailableListener {

  private static final int MAX_IMAGES = 3;
  private static final String TAG = "WebGPUAHBView";

  private final WebGPUAPI mApi;
  private ImageReader mImageReader;
  private Surface mSurface;
  private Image mCurrentImage;
  private Bitmap mCachedBitmap;
  private final Paint mPaint;
  private final Handler mHandler;
  private final Object mImageLock = new Object();
  private boolean mSurfaceCreated = false;
  private int mConfiguredWidth = 0;
  private int mConfiguredHeight = 0;

  public WebGPUAHBView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
    mPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    mHandler = new Handler(Looper.getMainLooper());

    // Enable hardware acceleration for this view
    setLayerType(LAYER_TYPE_HARDWARE, null);

    // Make sure we get drawn
    setWillNotDraw(false);
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    super.onSizeChanged(w, h, oldw, oldh);

    if (w > 0 && h > 0) {
      // Recreate ImageReader with new dimensions
      setupImageReader(w, h);
    }
  }

  private void setupImageReader(int width, int height) {
    // Don't recreate if dimensions haven't changed
    if (width == mConfiguredWidth && height == mConfiguredHeight && mImageReader != null) {
      return;
    }

    // Clean up previous ImageReader
    cleanupImageReader();

    try {
      // Create ImageReader with HardwareBuffer support
      mImageReader = ImageReader.newInstance(
        width,
        height,
        PixelFormat.RGBA_8888,
        MAX_IMAGES,
        HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
          HardwareBuffer.USAGE_GPU_COLOR_OUTPUT |
          HardwareBuffer.USAGE_COMPOSER_OVERLAY |
          HardwareBuffer.USAGE_CPU_READ_RARELY // For fallback to Bitmap if needed
      );

      mImageReader.setOnImageAvailableListener(this, mHandler);

      // Get the Surface for WebGPU to render to
      mSurface = mImageReader.getSurface();

      mConfiguredWidth = width;
      mConfiguredHeight = height;

      // Notify WebGPU about the new surface
      if (!mSurfaceCreated) {
        mApi.surfaceCreated(mSurface);
        mSurfaceCreated = true;
      } else {
        mApi.surfaceChanged(mSurface);
      }

    } catch (Exception e) {
      e.printStackTrace();
      // Fallback to offscreen if ImageReader creation fails
      mApi.surfaceOffscreen();
    }
  }

  @Override
  public void onImageAvailable(ImageReader reader) {
    synchronized (mImageLock) {
      // Close previous image if exists
      if (mCurrentImage != null) {
        mCurrentImage.close();
        mCurrentImage = null;
      }

      try {
        // Get the latest image
        mCurrentImage = reader.acquireLatestImage();

        if (mCurrentImage != null) {
          // Request a redraw on the UI thread
          postInvalidateOnAnimation();
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);

    synchronized (mImageLock) {
      if (mCurrentImage == null) {
        return;
      }

      try {
        // Try to use HardwareBuffer directly (most efficient path)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
          HardwareBuffer hardwareBuffer = mCurrentImage.getHardwareBuffer();

          if (hardwareBuffer != null) {
            // Draw using HardwareBuffer
            drawHardwareBuffer(canvas, hardwareBuffer);
            return;
          }
        }

        // Fallback: Convert Image to Bitmap
        drawImageAsBitmap(canvas, mCurrentImage);

      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  }

  private void drawHardwareBuffer(Canvas canvas, HardwareBuffer hardwareBuffer) {
    // On Android Q+, we can create a Bitmap from HardwareBuffer
    try {
      // Create or reuse bitmap with matching dimensions
      if (mCachedBitmap == null ||
        mCachedBitmap.getWidth() != hardwareBuffer.getWidth() ||
        mCachedBitmap.getHeight() != hardwareBuffer.getHeight()) {

        if (mCachedBitmap != null) {
          mCachedBitmap.recycle();
        }

        // Create a hardware-backed Bitmap from the HardwareBuffer
        mCachedBitmap = Bitmap.wrapHardwareBuffer(hardwareBuffer, null);
      } else {
        // Reuse existing bitmap and update with new HardwareBuffer content
        mCachedBitmap = Bitmap.wrapHardwareBuffer(hardwareBuffer, null);
      }

      if (mCachedBitmap != null) {
        // Draw the bitmap to canvas
        canvas.drawBitmap(mCachedBitmap, 0, 0, mPaint);
      }
    } catch (Exception e) {
      e.printStackTrace();
      // Fallback to Image-based rendering
      drawImageAsBitmap(canvas, mCurrentImage);
    }
  }

  private void drawImageAsBitmap(Canvas canvas, Image image) {
    // Fallback method: manually convert Image to Bitmap
    if (image.getFormat() != PixelFormat.RGBA_8888) {
      return;
    }

    Image.Plane[] planes = image.getPlanes();
    if (planes.length == 0) {
      return;
    }

    ByteBuffer buffer = planes[0].getBuffer();
    int pixelStride = planes[0].getPixelStride();
    int rowStride = planes[0].getRowStride();
    int rowPadding = rowStride - pixelStride * image.getWidth();

    // Create or reuse bitmap
    int bitmapWidth = image.getWidth() + rowPadding / pixelStride;
    if (mCachedBitmap == null ||
      mCachedBitmap.getWidth() != bitmapWidth ||
      mCachedBitmap.getHeight() != image.getHeight()) {

      if (mCachedBitmap != null) {
        mCachedBitmap.recycle();
      }
      mCachedBitmap = Bitmap.createBitmap(
        bitmapWidth,
        image.getHeight(),
        Bitmap.Config.ARGB_8888
      );
    }

    mCachedBitmap.copyPixelsFromBuffer(buffer);

    // Draw only the valid portion (without padding)
    Rect src = new Rect(0, 0, image.getWidth(), image.getHeight());
    Rect dst = new Rect(0, 0, getWidth(), getHeight());
    canvas.drawBitmap(mCachedBitmap, src, dst, mPaint);
  }

  private void cleanupImageReader() {
    synchronized (mImageLock) {
      if (mCurrentImage != null) {
        mCurrentImage.close();
        mCurrentImage = null;
      }

      if (mImageReader != null) {
        mImageReader.close();
        mImageReader = null;
      }

      if (mSurface != null) {
        mSurface.release();
        mSurface = null;
      }
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();

    // Notify WebGPU that surface is being destroyed
    if (mSurfaceCreated) {
      mApi.surfaceDestroyed();
      mSurfaceCreated = false;
    }

    // Clean up resources
    cleanupImageReader();

    if (mCachedBitmap != null) {
      mCachedBitmap.recycle();
      mCachedBitmap = null;
    }
  }

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();

    // Re-setup if we have valid dimensions
    if (getWidth() > 0 && getHeight() > 0) {
      setupImageReader(getWidth(), getHeight());
    }
  }

  @Override
  public void setAlpha(float alpha) {
    super.setAlpha(alpha);
    mPaint.setAlpha((int) (alpha * 255));
    invalidate();
  }

  @Override
  protected void onVisibilityChanged(@NonNull View changedView, int visibility) {
    super.onVisibilityChanged(changedView, visibility);

    if (visibility == VISIBLE && mSurface == null && getWidth() > 0 && getHeight() > 0) {
      // Re-create surface if needed when becoming visible
      setupImageReader(getWidth(), getHeight());
    }
  }
}
