package com.webgpu;

/**
 * A backing view that keeps the frame it shows while it is out of its window,
 * and shows it again when it comes back, as a CAMetalLayer and a web canvas
 * keep theirs.
 */
interface FrameKeeper {
  /**
   * The canvas will not attach the view again: it releases the frame it keeps
   * now, or, while it is still in its window, as it leaves. Main thread.
   */
  void retire();
}
