import type { RNCanvasContext } from "react-native-webgpu";
import { scheduleOnRN, scheduleOnUI } from "react-native-worklets";

// Test-harness helper (see packages/webgpu TextureViewPresent.spec.ts): clears
// the canvas to each color in turn from Reanimated's UI thread, the first two
// in the same animation frame and the rest one per frame, and resolves two
// frames after the last present, once the window has drawn it. This must live
// in app code: the "worklet" directive only works when the babel worklets
// plugin compiles it into the app bundle.
export const workletPresentFrames = (
  device: GPUDevice,
  context: RNCanvasContext,
  colors: GPUColor[],
): Promise<void> =>
  new Promise((resolve) => {
    scheduleOnUI(() => {
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
      const step = () => {
        if (next === 0) {
          present(colors[next++]);
        }
        if (next < colors.length) {
          present(colors[next++]);
          requestAnimationFrame(step);
          return;
        }
        requestAnimationFrame(() =>
          requestAnimationFrame(() => scheduleOnRN(resolve)),
        );
      };
      requestAnimationFrame(step);
    });
  });
