#pragma once

#import <Foundation/Foundation.h>

/**
 * The thread every canvas presents on, as on Android (see PresentThread.java)
 * and as React Native Skia's canvases do (RNSkPresentThread): one for the
 * process, started on first use and never stopped. A present copies the
 * canvas's frame onto the window's texture and presents it, which waits for
 * the window to hand a texture out and take it back; on the thread that
 * renders, that wait held the frames up. The thread has a run loop, which the
 * display links of the views tick on, and runs the tasks posted to it in
 * order: a view's display link is only touched here. CAMetalLayer takes
 * drawables and presents from any thread. A view attaches and detaches its
 * window here too, in order with the presents, and the main thread, which
 * only lays the window out (which never waits), never waits for one. The
 * window needs no mutex of its own for that: everything that touches it runs
 * here, one task after the other.
 */
@interface RNWGPresentThread : NSObject

/** Runs a task on the present thread, after those posted before it. Any
 * thread. */
+ (void)post:(dispatch_block_t)task;

@end
