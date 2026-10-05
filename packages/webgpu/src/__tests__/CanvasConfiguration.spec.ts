import { client } from "./setup";

describe("GPUCanvasConfiguration", () => {
  it("accepts native surface present modes", () => {
    const presentModes: NonNullable<GPUCanvasConfiguration["presentMode"]>[] = [
      "fifo",
      "fifo-relaxed",
      "immediate",
      "mailbox",
    ];

    const configuration = {
      device: {} as GPUDevice,
      format: "bgra8unorm",
      presentMode: "mailbox",
    } satisfies GPUCanvasConfiguration;

    expect(presentModes).toContain(configuration.presentMode);
  });

  it("rejects unknown present modes", () => {
    const configuration: GPUCanvasConfiguration = {
      device: {} as GPUDevice,
      format: "bgra8unorm",
      // @ts-expect-error presentMode only accepts Dawn surface modes.
      presentMode: "triple-buffered",
    };

    expect(configuration).toBeDefined();
  });

  it("configures a native surface with an explicit FIFO present mode", async () => {
    if (client.OS !== "ios" && client.OS !== "android") {
      return;
    }

    const size = await client.eval(({ ctx, device, gpu }) => {
      const nativeContext = ctx as GPUCanvasContext & { present(): void };
      nativeContext.configure({
        device,
        format: gpu.getPreferredCanvasFormat(),
        presentMode: "fifo",
      });
      const texture = nativeContext.getCurrentTexture();
      const result = { width: texture.width, height: texture.height };
      nativeContext.present();
      return result;
    });

    expect(size.width).toBeGreaterThan(0);
    expect(size.height).toBeGreaterThan(0);
  });
});
