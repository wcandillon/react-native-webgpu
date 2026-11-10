package com.webgpu;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;

import java.util.concurrent.atomic.AtomicBoolean;

import com.facebook.proguard.annotations.DoNotStrip;

/**
 * The thread every canvas presents on, as React Native Skia's canvases do
 * (its PresentThread): one for the process, started on first use and never
 * stopped. Presenting a frame (copying it onto the window's buffer and handing
 * the buffer to the compositor, or to the window for a TextureView) waits for
 * the window to hand a buffer out and take it back, which on a Vulkan window
 * waits for the GPU; this thread takes that off the thread that renders and
 * off the main thread. One thread serves every canvas, as presents serialize
 * on the GPU anyway. It is a looper thread, so that each canvas's frame
 * scheduler (rnsurface::FrameScheduler, on the native side) runs on it as it
 * would on the main thread, with the Choreographer of this thread for its
 * vsync (see WebGPUView for how a TextureView's present still waits for its
 * window's pending draw). Native code posts to it through post().
 */
final class PresentThread {
    private static final Object sLock = new Object();
    private static Handler sHandler;

    // Whether the thread is known to be held up: a main-thread wait for a task
    // posted here timed out (see WebGPUView.onSurfaceDestroyed), and that task
    // has not run since. The present in flight is then waiting on a pipeline
    // compile (seconds on a slow GPU, see #4394), and no other wait for the
    // thread is worth making until it drains: each would time out in turn,
    // and the main thread would pay the timeout once per canvas. The task
    // waited for clears the flag when the thread gets to it, or the wait does
    // when the task got there first (see WebGPUView.onSurfaceDestroyed): the
    // flag never outlives the task.
    private static final AtomicBoolean sHeldUp = new AtomicBoolean(false);

    private PresentThread() {
    }

    /**
     * Runs a task on the thread, after those posted before it, never on the
     * spot. Any thread; called from native (AndroidPlatformContext).
     */
    @DoNotStrip
    static void post(Runnable task) {
        handler().post(task);
    }

    /** The handler of the thread, started on first use. Any thread. */
    static Handler handler() {
        synchronized (sLock) {
            if (sHandler == null) {
                // The priority of the UI toolkit's own render thread.
                HandlerThread thread = new HandlerThread("RNWebGPU Present",
                        Process.THREAD_PRIORITY_DISPLAY);
                thread.start();
                sHandler = new Handler(thread.getLooper());
            }
            return sHandler;
        }
    }

    /**
     * Whether a wait for this thread timed out and the thread has not run
     * the task waited for since. Any thread.
     */
    static boolean isHeldUp() {
        return sHeldUp.get();
    }

    /**
     * Records a wait for this thread that timed out (true, from the waiting
     * thread) or the thread getting to the task waited for (false, from this
     * thread).
     */
    static void setHeldUp(boolean heldUp) {
        sHeldUp.set(heldUp);
    }
}
