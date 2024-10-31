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
import android.util.Log;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.util.LinkedList;

@SuppressLint("ViewConstructor")
@RequiresApi(api = Build.VERSION_CODES.Q)
public class WebGPUAHBView extends View {

  private ImageReader mReader = null;
  private ImageReader mOldReader = null;

  private Bitmap mBitmap = null;

  private final Matrix matrix = new Matrix();

  WebGPUAPI mApi;

  public WebGPUAHBView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
  }

  private ImageReader createReader() {
    return ImageReader.newInstance(getWidth(), getHeight(), PixelFormat.RGBA_8888, 2, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    if (mReader == null) {
      mReader = createReader();
      mApi.surfaceCreated(mReader.getSurface());
    } else {
      mApi.surfaceChanged(mReader.getSurface());
    }
  }

  public void present() {
    try (Image image = mReader.acquireLatestImage()) {
      if (image != null) {
        HardwareBuffer hb = image.getHardwareBuffer();
        if (hb != null) {
          Bitmap bitmap = Bitmap.wrapHardwareBuffer(hb, null);
          if (bitmap != null) {
            Log.i("WebGPUView", "size: " + bitmap.getWidth() + "x" + bitmap.getHeight());
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
      canvas.drawBitmap(mBitmap, matrix, null);
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    mApi.surfaceDestroyed();
  }
}
