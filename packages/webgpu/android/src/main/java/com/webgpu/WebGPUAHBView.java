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

import java.util.LinkedList;

@SuppressLint("ViewConstructor")
@RequiresApi(api = Build.VERSION_CODES.Q)
public class WebGPUAHBView extends View {

  private ImageReader mReader = null;
  private ImageReader mOldReader = null;
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
    return ImageReader.newInstance(mWidth, mHeight, PixelFormat.RGBA_8888, 2, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    mWidth = getWidth();
    mHeight = getHeight();
    if (mReader == null) {
      mReader = createReader();
      mApi.surfaceCreated(mReader.getSurface());
    }
  }

  public void present() {
    try (Image image = mReader.acquireLatestImage()) {
      if (image != null) {
        HardwareBuffer hb = image.getHardwareBuffer();
        if (hb != null) {
          Bitmap bitmap = Bitmap.wrapHardwareBuffer(hb, null);
          if (bitmap != null) {
            mBitmap = bitmap;
            hb.close();
            invalidate();
            boolean hasResized = mWidth != mReader.getWidth() || mHeight != mReader.getHeight();
            if (hasResized) {
              mOldReader = mReader;
              mReader = createReader();
              mApi.surfaceChanged(mReader.getSurface());
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
