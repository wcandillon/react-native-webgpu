import { client } from "./setup";

describe("Native canvas context", () => {
  it("returns the same texture within a frame, even after destroy", async () => {
    if (client.OS !== "ios" && client.OS !== "android") {
      return;
    }
    const result = await client.eval(({ device, gpu }) => {
      // The harness uses a JS offscreen polyfill; exercise the native context.
      const ctx = RNWebGPU.MakeWebGPUCanvasContext(-1, 16, 16);
      ctx.configure({ device, format: gpu.getPreferredCanvasFormat() });
      try {
        const texture = ctx.getCurrentTexture();
        const sameTexture = texture === ctx.getCurrentTexture();
        texture.destroy();
        return sameTexture && texture === ctx.getCurrentTexture();
      } finally {
        ctx.unconfigure();
        RNWebGPU.destroyContext(-1);
      }
    });
    expect(result).toBe(true);
  });

  it.each(["present", "configure", "unconfigure", "resize"] as const)(
    "returns a new texture after %s",
    async (operation) => {
      if (client.OS !== "ios" && client.OS !== "android") {
        return;
      }
      const result = await client.eval(
        ({ device, gpu, operation: transition }) => {
          const ctx = RNWebGPU.MakeWebGPUCanvasContext(-1, 16, 16);
          const config = { device, format: gpu.getPreferredCanvasFormat() };
          ctx.configure(config);
          try {
            const previous = ctx.getCurrentTexture();
            switch (transition) {
              case "present":
                ctx.present();
                break;
              case "configure":
                ctx.configure(config);
                break;
              case "unconfigure":
                ctx.unconfigure();
                ctx.configure(config);
                break;
              case "resize":
                ctx.canvas.width = 32;
                break;
            }
            const current = ctx.getCurrentTexture();
            return {
              changed: current !== previous,
              stable: current === ctx.getCurrentTexture(),
              width: current.width,
            };
          } finally {
            ctx.unconfigure();
            RNWebGPU.destroyContext(-1);
          }
        },
        { operation },
      );
      expect(result).toEqual({
        changed: true,
        stable: true,
        width: operation === "resize" ? 32 : 16,
      });
    },
  );
});
