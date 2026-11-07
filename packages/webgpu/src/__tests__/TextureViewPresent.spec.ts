import { client } from "./setup";

// The harness mounts a 32x32pt canvas backed by a TextureView on Android
// (apps/example/src/Tests.tsx). A frame presented on the UI thread reaches it
// behind the window's pending draw; this checks that the window shows the last
// frame presented, also after a frame acquired while the previous one still
// waited for its present.

type RGBA = [number, number, number, number];

const LIME: RGBA = [0, 255, 0, 255];

const expectColor = (actual: number[], expected: RGBA, tolerance = 3) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(tolerance);
  });
};

describe("TextureView canvas", () => {
  it("shows the last frame a worklet presents on the UI thread", async () => {
    if (client.OS !== "android") {
      return;
    }
    const result = await client.eval(
      ({ device, views, workletPresentFrames }) => {
        const context = views?.textureCanvas()?.getContext("webgpu");
        const source = views?.textureCanvasView;
        if (!views || !context || !source || !workletPresentFrames) {
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
        return workletPresentFrames(device, context, colors)
          .then(() =>
            device.queue.drawElementImageToTexture({ source }, { texture }),
          )
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
            return pixel;
          });
      },
    );
    expectColor(result, LIME);
  });
});
