import { client } from "./setup";

// The harness mounts a 32x32pt canvas backed by a TextureView on Android
// (apps/example/src/Tests.tsx). A frame presented on the UI thread reaches it
// behind the window's pending draw, and on Android 11 a frame presented before
// the window drew the previous one is copied in once it has; this checks that
// the window shows the last frame presented from either thread, also after a
// frame acquired while the previous one still waited for its present, and that
// presenting never stalls the thread.

type RGBA = [number, number, number, number];

const LIME: RGBA = [0, 255, 0, 255];

// An acquire that waits for the window's draw on the thread that draws it
// stalls that thread for Android 11's 4.29 s dequeue timeout.
const FRAME_GAP_BUDGET_MS = 2000;

const expectColor = (actual: number[], expected: RGBA, tolerance = 3) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(tolerance);
  });
};

describe("TextureView canvas", () => {
  it.each(["UI", "JS"] as const)(
    "shows the last frame presented on the %s thread without stalling it",
    async (presentingThread) => {
      if (client.OS !== "android") {
        return;
      }
      const result = await client.eval(
        ({ device, views, presentFrames, thread }) => {
          const context = views?.textureCanvas()?.getContext("webgpu");
          const source = views?.textureCanvasView;
          if (!views || !context || !source || !presentFrames) {
            throw new Error("The Tests screen has no TextureView canvas");
          }
          context.configure({
            device,
            format: navigator.gpu.getPreferredCanvasFormat(),
            alphaMode: "opaque",
          });
          const size = Math.round(views.textureCanvasSize * views.pixelRatio);
          const texture = device.createTexture({
            size: [size, size],
            format: "rgba8unorm",
            usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
          });
          const bytesPerRow = Math.ceil((size * 4) / 256) * 256;
          const output = device.createBuffer({
            size: bytesPerRow * size,
            usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
          });
          const colors: GPUColor[] = [
            [1, 0, 0, 1],
            [1, 1, 0, 1],
            [0, 1, 1, 1],
            [1, 0, 1, 1],
            [0, 1, 0, 1],
          ];
          let longestFrameGapMs = 0;
          return presentFrames(device, context, colors, thread)
            .then((frameGapMs) => {
              longestFrameGapMs = frameGapMs;
              return device.queue.drawElementImageToTexture(
                { source },
                { texture },
              );
            })
            .then(() => {
              const encoder = device.createCommandEncoder();
              encoder.copyTextureToBuffer(
                { texture },
                { buffer: output, bytesPerRow },
                [size, size],
              );
              device.queue.submit([encoder.finish()]);
              return output.mapAsync(GPUMapMode.READ);
            })
            .then(() => {
              const data = new Uint8Array(output.getMappedRange());
              const centre = (size / 2) * bytesPerRow + (size / 2) * 4;
              const pixel = Array.from(data.slice(centre, centre + 4));
              output.unmap();
              return { pixel, longestFrameGapMs };
            });
        },
        { thread: presentingThread },
      );
      expectColor(result.pixel, LIME);
      expect(result.longestFrameGapMs).toBeLessThan(FRAME_GAP_BUDGET_MS);
    },
  );
});
