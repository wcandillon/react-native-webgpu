import type { RNCanvasContext } from "react-native-webgpu";
import { scheduleOnRN, scheduleOnUI } from "react-native-worklets";

// Test-harness helper (see packages/webgpu TextureViewPresent.spec.ts): clears
// the canvas to each color in turn, the first two in the same animation frame
// and the rest one per frame, and calls done two frames after the last
// present, once the window has drawn it, with the longest time between two of
// those frames. This must live in app code: the "worklet" directive only works
// when the babel worklets plugin compiles it into the app bundle.
const presentColors = (
  device: GPUDevice,
  context: RNCanvasContext,
  colors: GPUColor[],
  done: (longestFrameGapMs: number) => void,
) => {
  "worklet";
  const present = (color: GPUColor) => {
    const encoder = device.createCommandEncoder();
    encoder
      .beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: color,
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      })
      .end();
    device.queue.submit([encoder.finish()]);
    context.present();
  };
  let next = 0;
  let previousFrameMs = -1;
  let longestFrameGapMs = 0;
  const onFrame = (callback: () => void) => () => {
    const nowMs = performance.now();
    if (previousFrameMs >= 0) {
      longestFrameGapMs = Math.max(longestFrameGapMs, nowMs - previousFrameMs);
    }
    previousFrameMs = nowMs;
    callback();
  };
  const step = () => {
    if (next === 0) {
      present(colors[next++]);
    }
    if (next < colors.length) {
      present(colors[next++]);
      requestAnimationFrame(onFrame(step));
      return;
    }
    requestAnimationFrame(
      onFrame(() =>
        requestAnimationFrame(onFrame(() => done(longestFrameGapMs))),
      ),
    );
  };
  requestAnimationFrame(onFrame(step));
};

// Presents those frames from Reanimated's UI thread or from the JS thread.
export const presentFrames = (
  device: GPUDevice,
  context: RNCanvasContext,
  colors: GPUColor[],
  thread: "UI" | "JS",
): Promise<number> =>
  new Promise((resolve) => {
    if (thread === "JS") {
      presentColors(device, context, colors, resolve);
      return;
    }
    scheduleOnUI(() => {
      "worklet";
      presentColors(device, context, colors, (longestFrameGapMs) =>
        scheduleOnRN(resolve, longestFrameGapMs),
      );
    });
  });
