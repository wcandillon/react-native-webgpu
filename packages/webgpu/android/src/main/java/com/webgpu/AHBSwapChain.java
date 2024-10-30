package com.webgpu;

import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.hardware.HardwareBuffer;
import android.os.Build;

import androidx.annotation.RequiresApi;

@RequiresApi(api = Build.VERSION_CODES.Q)
public class AHBSwapChain {
  private Bitmap mBitmap = null;
  private HardwareBuffer readBuffer = null;
  private HardwareBuffer writeBuffer = null;
  private int mWidth = 0;
  private int mHeight = 0;

  private final Object lock = new Object();

  AHBSwapChain() {}

  Bitmap getBitmap() {
    synchronized (lock) {
      return mBitmap;
    }
  }

  HardwareBuffer getBuffer() {
    return writeBuffer;
  }

  void layout(int width, int height) {
    synchronized (lock) {
      mWidth = width;
      mHeight = height;
      if (readBuffer == null) {
        initBuffers();
      }
    }
  }

  void present() {
    synchronized (lock) {
      boolean hasResized = mWidth != writeBuffer.getWidth() || mHeight != writeBuffer.getHeight();
      HardwareBuffer swap = readBuffer;
      readBuffer = writeBuffer;
      writeBuffer = swap;
      mBitmap = Bitmap.wrapHardwareBuffer(readBuffer, null);
      if (hasResized) {
        initBuffers();
      }
    }
  }

  private void initBuffers() {
    readBuffer = createBuffer(mWidth, mHeight);
    writeBuffer = createBuffer(mWidth, mHeight);
  }

  private static HardwareBuffer createBuffer(int width, int height) {
    return HardwareBuffer.create(width, height, HardwareBuffer.RGBA_8888, 1, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE |
      HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
  }

}
