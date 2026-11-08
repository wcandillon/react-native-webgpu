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

  // React's StrictMode runs the Canvas cleanup once right after mount and
  // keeps the context the first effect run made. A native view attaching its
  // surface after that cleanup must find the SurfaceInfo that context holds,
  // not a new one, so the registry keeps a retired SurfaceInfo for as long as
  // a context holds it. The attach is a find-or-create by id like a second
  // MakeWebGPUCanvasContext, which stands in for it here: the frame epoch is
  // per SurfaceInfo, so presenting through one context ends the other's frame
  // only when they share it.
  it("keeps the surface of a destroyed context for its id while the context lives", async () => {
    if (client.OS !== "ios" && client.OS !== "android") {
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

  // A frame ends with present(), and the next getCurrentTexture() starts a new
  // one, at the drawing buffer's size and format. A canvas presenting direct
  // (the default) renders into the window's own texture, or into a texture
  // standing in for it without a window, with exactly the usage it was
  // configured with. A composited canvas renders into offscreen frames of its
  // own, which present() hands to the present thread (see SurfaceInfo): they
  // carry the usage the canvas was configured with and what presenting them
  // needs.
  it.each([
    { presentation: "direct", contextId: -5, exactUsage: true },
    { presentation: "composited", contextId: -6, exactUsage: false },
  ])(
    "starts a new frame after present(), at the drawing buffer's size ($presentation)",
    async ({ presentation: mode, contextId: id, exactUsage }) => {
      if (client.OS !== "ios" && client.OS !== "android") {
        return;
      }
      const result = await client.eval(
        ({ device, gpu, presentation, contextId }) => {
          const format = gpu.getPreferredCanvasFormat();
          const usage =
            GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_DST;
          const ctx = RNWebGPU.MakeWebGPUCanvasContext(
            contextId,
            16,
            16,
            presentation === "composited",
          );
          ctx.configure({ device, format, usage });
          try {
            let distinctFromPrevious = true;
            let describesTheBuffer = true;
            let usageIsExact = true;
            let previous: GPUTexture | null = null;
            for (let frame = 0; frame < 6; frame++) {
              const texture = ctx.getCurrentTexture();
              if (texture === previous) {
                distinctFromPrevious = false;
              }
              if (
                texture.width !== 16 ||
                texture.height !== 16 ||
                texture.format !== format ||
                (texture.usage & usage) !== usage
              ) {
                describesTheBuffer = false;
              }
              if (texture.usage !== usage) {
                usageIsExact = false;
              }
              const encoder = device.createCommandEncoder();
              const pass = encoder.beginRenderPass({
                colorAttachments: [
                  {
                    view: texture.createView(),
                    clearValue: [frame / 6, 0, 0, 1],
                    loadOp: "clear",
                    storeOp: "store",
                  },
                ],
              });
              pass.end();
              device.queue.submit([encoder.finish()]);
              ctx.present();
              previous = texture;
            }
            return { distinctFromPrevious, describesTheBuffer, usageIsExact };
          } finally {
            ctx.unconfigure();
            RNWebGPU.destroyContext(contextId);
          }
        },
        { presentation: mode, contextId: id },
      );
      expect(result).toEqual({
        distinctFromPrevious: true,
        describesTheBuffer: true,
        usageIsExact: exactUsage,
      });
    },
  );

  // A composited canvas's frames are presented from the present thread, which
  // only a device with implicit device synchronization may be used from; a
  // direct canvas presents on the thread that renders, with any device.
  it("configures a direct canvas, not a composited one, with a device that does not synchronize", async () => {
    if (client.OS !== "ios" && client.OS !== "android") {
      return;
    }
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter()
        .then((adapter) =>
          adapter!.requestDevice({ implicitDeviceSynchronization: false }),
        )
        .then((device) => {
          const format = gpu.getPreferredCanvasFormat();
          const direct = RNWebGPU.MakeWebGPUCanvasContext(-7, 16, 16, false);
          const composited = RNWebGPU.MakeWebGPUCanvasContext(-8, 16, 16, true);
          try {
            direct.configure({ device, format });
            const texture = direct.getCurrentTexture();
            direct.present();
            const directRenders =
              texture.width === 16 && direct.getCurrentTexture() !== texture;
            let compositedRefused = false;
            try {
              composited.configure({ device, format });
            } catch (e) {
              compositedRefused = `${(e as Error).message}`.includes(
                "implicit device synchronization",
              );
            }
            return { directRenders, compositedRefused };
          } finally {
            direct.unconfigure();
            composited.unconfigure();
            RNWebGPU.destroyContext(-7);
            RNWebGPU.destroyContext(-8);
            device.destroy();
          }
        });
    });
    expect(result).toEqual({ directRenders: true, compositedRefused: true });
  });

  it.each(["srgb", "display-p3"] as const)(
    "configures a native canvas with colorSpace %s",
    async (colorSpace) => {
      if (client.OS !== "ios" && client.OS !== "android") {
        return;
      }
      const result = await client.eval(
        ({ device, gpu, colorSpace: space }) => {
          const ctx = RNWebGPU.MakeWebGPUCanvasContext(-3, 16, 16);
          try {
            // Every canvas format the library offers, so the layer tagging
            // runs for the 8-bit and the float path alike.
            for (const format of [
              gpu.getPreferredCanvasFormat(),
              "rgba16float" as GPUTextureFormat,
            ]) {
              ctx.configure({ device, format, colorSpace: space });
              if (ctx.getCurrentTexture().format !== format) {
                return false;
              }
            }
            return true;
          } finally {
            ctx.unconfigure();
            RNWebGPU.destroyContext(-3);
          }
        },
        { colorSpace },
      );
      expect(result).toBe(true);
    },
  );

  it.each(["standard", "extended"] as const)(
    "configures a native canvas with toneMapping mode %s",
    async (mode) => {
      if (client.OS !== "ios" && client.OS !== "android") {
        return;
      }
      const result = await client.eval(
        ({ device, gpu, mode: toneMappingMode }) => {
          const ctx = RNWebGPU.MakeWebGPUCanvasContext(-3, 16, 16);
          try {
            // Both formats: the extended range only applies to the float
            // one, the 8-bit one must accept the mode and ignore it.
            for (const format of [
              gpu.getPreferredCanvasFormat(),
              "rgba16float" as GPUTextureFormat,
            ]) {
              ctx.configure({
                device,
                format,
                toneMapping: { mode: toneMappingMode },
              });
              if (ctx.getCurrentTexture().format !== format) {
                return false;
              }
            }
            // Back to the default so the layer leaves the extended range.
            ctx.configure({ device, format: "rgba16float" });
            return ctx.getCurrentTexture().format === "rgba16float";
          } finally {
            ctx.unconfigure();
            RNWebGPU.destroyContext(-3);
          }
        },
        { mode },
      );
      expect(result).toBe(true);
    },
  );

  it("rejects an unknown toneMapping mode", async () => {
    if (client.OS !== "ios" && client.OS !== "android") {
      return;
    }
    const result = await client.eval(({ device, gpu }) => {
      const ctx = RNWebGPU.MakeWebGPUCanvasContext(-3, 16, 16);
      try {
        ctx.configure({
          device,
          format: gpu.getPreferredCanvasFormat(),
          toneMapping: { mode: "hdr" as GPUCanvasToneMappingMode },
        });
        return "configured";
      } catch (e) {
        return e instanceof Error ? e.message : String(e);
      } finally {
        ctx.unconfigure();
        RNWebGPU.destroyContext(-3);
      }
    });
    expect(result).toMatch(/GPUCanvasToneMappingMode/);
  });

  it("rejects an unknown colorSpace", async () => {
    if (client.OS !== "ios" && client.OS !== "android") {
      return;
    }
    const result = await client.eval(({ device, gpu }) => {
      const ctx = RNWebGPU.MakeWebGPUCanvasContext(-3, 16, 16);
      try {
        ctx.configure({
          device,
          format: gpu.getPreferredCanvasFormat(),
          colorSpace: "rec2020" as PredefinedColorSpace,
        });
        return "configured";
      } catch (e) {
        return e instanceof Error ? e.message : String(e);
      } finally {
        ctx.unconfigure();
        RNWebGPU.destroyContext(-3);
      }
    });
    expect(result).toMatch(/PredefinedColorSpace/);
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
