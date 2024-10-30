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

  private ImageReader mReader1;
  private ImageReader mReader2;
  private int mWidth = 0;
  private int mHeight = 0;

  private Bitmap mBitmap = null;

  private final Matrix matrix = new Matrix();

  WebGPUAPI mApi;

  public WebGPUAHBView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
  }

  private ImageReader createReader() {
    ImageReader reader = ImageReader.newInstance(mWidth, mHeight, PixelFormat.RGBA_8888, 2, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
    reader.setOnImageAvailableListener(this, null);
    return reader;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    mWidth = getWidth();
    mHeight = getHeight();
    if (mReader1 == null) {
      mReader1 = createReader();
      mApi.surfaceCreated(mReader1.getSurface());
    }
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
            boolean hasResized = mWidth != reader.getWidth() || mHeight != reader.getHeight();
            if (hasResized) {
              ImageReader newReader = createReader();
              mApi.surfaceChanged(newReader.getSurface());
              if (reader == mReader1) {
                mReader2 = newReader;
              } else {
                mReader1 = newReader;
              }
            }
          }
        }
      }
    }
  }

  @Override
  protected void onDraw(@NonNull Canvas canvas) {
    super.onDraw(canvas);
    if (mBitmap != null) {
      canvas.drawBitmap(mBitmap, matrix, null);
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    mApi.surfaceDestroyed();
  }
}
