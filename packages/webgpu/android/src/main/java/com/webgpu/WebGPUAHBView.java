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
import java.util.Queue;

@SuppressLint("ViewConstructor")
@RequiresApi(api = Build.VERSION_CODES.Q)
public class WebGPUAHBView extends View {

  private final Queue<ImageReader> mImageReaders = new LinkedList<>();
  private Bitmap mBitmap = null;

  private final Matrix matrix = new Matrix();

  WebGPUAPI mApi;

  public WebGPUAHBView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    long usage = HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT;
    ImageReader imageReader = ImageReader.newInstance(getWidth(), getHeight(), PixelFormat.RGBA_8888, 2, usage);
    if (mImageReaders.isEmpty()) {
      mApi.surfaceCreated(imageReader.getSurface());
    } else {
      mApi.surfaceChanged(imageReader.getSurface());
    }
    imageReader.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {
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
                ImageReader imageReader = mImageReaders.poll();
                ImageReader ir;
                while((ir = mImageReaders.poll()) != null) {
                    ir.close();
                }
                mImageReaders.add(imageReader);
              }
            }
          }
        }
      }
    }, null);
    mImageReaders.add(imageReader);
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
