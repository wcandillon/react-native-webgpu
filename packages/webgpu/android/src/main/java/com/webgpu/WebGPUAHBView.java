package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.PixelFormat;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

@SuppressLint("ViewConstructor")
@RequiresApi(api = Build.VERSION_CODES.Q)
public class WebGPUAHBView extends View implements ImageReader.OnImageAvailableListener {

  private ImageReader mReader;

  private Bitmap mBitmap = null;

  private final Matrix matrix = new Matrix();

  WebGPUAPI mApi;

  public WebGPUAHBView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
  }

  private static ImageReader createReader(int width, int height, ImageReader.OnImageAvailableListener listener) {
    ImageReader reader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
    reader.setOnImageAvailableListener(listener, null);
    return reader;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);

    if (mReader == null) {
      int width = getWidth();
      int height = getHeight();

      if (width == 0 || height == 0) return;

      mReader = WebGPUAHBView.createReader(width, height, this);
    }

    mApi.surfaceChanged(mReader.getSurface());
  }

  @Override
  public void onImageAvailable(ImageReader reader) {
    try (Image image = reader.acquireLatestImage()) {
      if (image != null) {
        HardwareBuffer hb = image.getHardwareBuffer();
        if (hb != null) {
          Bitmap bitmap = Bitmap.wrapHardwareBuffer(hb, null);
          if (bitmap != null) {
            mBitmap = bitmap;
            hb.close();
            invalidate();
          }
        }
      }
    }
  }

  @Override
  protected void onDraw(@NonNull Canvas canvas) {
    super.onDraw(canvas);
    if (mBitmap != null) {
      float viewWidth = getWidth();
      float viewHeight = getHeight();
      float bitmapWidth = mBitmap.getWidth();
      float bitmapHeight = mBitmap.getHeight();

      // Calculate the scale factors
      float scaleX = viewWidth / bitmapWidth;
      float scaleY = viewHeight / bitmapHeight;

      // Reset the matrix and apply scaling
      matrix.reset();
      matrix.setScale(scaleX, scaleY);

      canvas.drawBitmap(mBitmap, matrix, null);
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    mApi.surfaceDestroyed();
  }
}
