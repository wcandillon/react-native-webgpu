import { client, isNativeApp } from "./setup";

describe("Native canvas context", () => {
  it("returns the same texture within a frame, even after destroy", async () => {
    if (!isNativeApp()) {
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

  // React's StrictMode runs the Canvas cleanup once right after mount and
  // keeps the context the first effect run made. A native view attaching its
  // surface after that cleanup must find the SurfaceInfo that context holds,
  // not a new one, so the registry keeps a retired SurfaceInfo for as long as
  // a context holds it. The attach is a find-or-create by id like a second
  // MakeWebGPUCanvasContext, which stands in for it here: the frame epoch is
  // per SurfaceInfo, so presenting through one context ends the other's frame
  // only when they share it.
  it("keeps the surface of a destroyed context for its id while the context lives", async () => {
    if (!isNativeApp()) {
      return;
    }
    const result = await client.eval(({ device, gpu }) => {
      const config = { device, format: gpu.getPreferredCanvasFormat() };
      const kept = RNWebGPU.MakeWebGPUCanvasContext(-2, 16, 16);
      kept.configure(config);
      RNWebGPU.destroyContext(-2);
      const next = RNWebGPU.MakeWebGPUCanvasContext(-2, 16, 16);
      next.configure(config);
      try {
        const texture = next.getCurrentTexture();
        kept.present();
        return next.getCurrentTexture() !== texture;
      } finally {
        next.unconfigure();
        kept.unconfigure();
        RNWebGPU.destroyContext(-2);
      }
    });
    expect(result).toBe(true);
  });

  it.each(["present", "configure", "unconfigure", "resize"] as const)(
    "returns a new texture after %s",
    async (operation) => {
      if (!isNativeApp()) {
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
