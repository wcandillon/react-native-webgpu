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
public class WebGPUAHBView extends View {

  private ImageReader mImageReader = null;
  private Bitmap mBitmap = null;

  private final Matrix matrix = new Matrix();

  WebGPUAPI mApi;

  public WebGPUAHBView(Context context, WebGPUAPI api) {
    super(context);
    setWillNotDraw(false);
    mApi = api;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    long usage = HardwareBuffer.USAGE_CPU_READ_RARELY |
      HardwareBuffer.USAGE_CPU_WRITE_RARELY |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT |
      HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE;
    mImageReader = ImageReader.newInstance(getWidth(), getHeight(), PixelFormat.RGBA_8888, 2, usage);
    mApi.surfaceCreated(mImageReader.getSurface());
    mImageReader.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {
      @Override
      public void onImageAvailable(ImageReader reader) {
        try (Image image = mImageReader.acquireLatestImage()) {
          if (image != null) {
            //start = System.nanoTime();
            HardwareBuffer hb = image.getHardwareBuffer();
            if (hb != null) {
              mBitmap = Bitmap.wrapHardwareBuffer(hb, null);
              hb.close();
            }
            invalidate();
          }
        }
      }
    }, null);
  }

  @Override
  protected void onDraw(@NonNull Canvas canvas) {
    super.onDraw(canvas);
    if (mBitmap != null) {
      //end = System.nanoTime();
      //Log.i(tag, "render time: " + (end - start) / 1000000 + "ms");
      canvas.drawBitmap(mBitmap, matrix, null);
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    mApi.surfaceDestroyed();
  }
}
