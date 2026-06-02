package com.webgpu;

import android.os.Handler;
import android.os.Looper;
import android.view.Choreographer;

/**
 * Drives WebGPU auto-present from the main-thread {@link Choreographer},
 * replacing the manual {@code context.present()} call.
 *
 * <p>{@link #start()} / {@link #stop()} are invoked from native code
 * (rnwgpu::FrameDriver::setPlatformVSync) on arbitrary threads; both hop to the
 * main thread. While running, {@link #doFrame(long)} calls back into native
 * once per vsync, where pending surfaces are presented.
 */
public class WebGPUFrameDriver implements Choreographer.FrameCallback {
  private static final WebGPUFrameDriver INSTANCE = new WebGPUFrameDriver();

  private final Handler mainHandler = new Handler(Looper.getMainLooper());
  private boolean running = false;

  private WebGPUFrameDriver() {}

  /** Called from native (any thread). */
  public static void start() {
    INSTANCE.startInternal();
  }

  /** Called from native (any thread). */
  public static void stop() {
    INSTANCE.stopInternal();
  }

  private void startInternal() {
    mainHandler.post(
        () -> {
          if (running) {
            return;
          }
          running = true;
          Choreographer.getInstance().postFrameCallback(this);
        });
  }

  private void stopInternal() {
    mainHandler.post(
        () -> {
          if (!running) {
            return;
          }
          running = false;
          Choreographer.getInstance().removeFrameCallback(this);
        });
  }

  @Override
  public void doFrame(long frameTimeNanos) {
    if (!running) {
      return;
    }
    nativeOnVSync();
    Choreographer.getInstance().postFrameCallback(this);
  }

  private static native void nativeOnVSync();
}
