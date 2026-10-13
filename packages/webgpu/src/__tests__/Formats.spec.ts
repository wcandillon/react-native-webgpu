import { client } from "./setup";

// Formats the spec added after the initial conversion tables were written:
// the 16-bit normalized texture formats behind "texture-formats-tier1" and the
// single-component and BGRA vertex formats.
describe("Formats", () => {
  it("creates textures in the texture-formats-tier1 formats", async () => {
    const formats: GPUTextureFormat[] = [
      "r16unorm",
      "r16snorm",
      "rg16unorm",
      "rg16snorm",
      "rgba16unorm",
      "rgba16snorm",
    ];
    const result = await client.eval<
      { formats: GPUTextureFormat[] },
      GPUTextureFormat[] | "unsupported"
    >(
      ({ gpu, formats: requested }) => {
        return gpu
          .requestAdapter()
          .then((adapter): Promise<GPUTextureFormat[] | "unsupported"> => {
            if (!adapter || !adapter.features.has("texture-formats-tier1")) {
              return Promise.resolve("unsupported");
            }
            return adapter
              .requestDevice({ requiredFeatures: ["texture-formats-tier1"] })
              .then((device) =>
                requested.map((format) => {
                  const texture = device.createTexture({
                    size: [4, 4],
                    format,
                    usage:
                      GPUTextureUsage.TEXTURE_BINDING |
                      GPUTextureUsage.COPY_DST,
                  });
                  return texture.format;
                }),
              );
          });
      },
      { formats },
    );
    if (result === "unsupported") {
      console.log("Formats: texture-formats-tier1 is not supported, skipping");
      return;
    }
    expect(result).toEqual(formats);
  });

  it("accepts the single-component and bgra vertex formats", async () => {
    const result = await client.eval(({ device }) => {
      const module = device.createShaderModule({
        code: `
          struct In {
            @location(0) bgra: vec4f,
            @location(1) u8: u32,
            @location(2) s8: i32,
            @location(3) un8: f32,
            @location(4) sn8: f32,
            @location(5) u16: u32,
            @location(6) s16: i32,
            @location(7) un16: f32,
            @location(8) sn16: f32,
            @location(9) f16: f32,
          }
          @vertex fn vs(in: In) -> @builtin(position) vec4f {
            let sum = in.bgra.x + f32(in.u8) + f32(in.s8) + in.un8 + in.sn8 +
              f32(in.u16) + f32(in.s16) + in.un16 + in.sn16 + in.f16;
            return vec4f(sum, 0, 0, 1);
          }
          @fragment fn fs() -> @location(0) vec4f { return vec4f(1); }
        `,
      });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module,
          buffers: [
            {
              arrayStride: 20,
              attributes: [
                { shaderLocation: 0, offset: 0, format: "unorm8x4-bgra" },
                { shaderLocation: 1, offset: 4, format: "uint8" },
                { shaderLocation: 2, offset: 5, format: "sint8" },
                { shaderLocation: 3, offset: 6, format: "unorm8" },
                { shaderLocation: 4, offset: 7, format: "snorm8" },
                { shaderLocation: 5, offset: 8, format: "uint16" },
                { shaderLocation: 6, offset: 10, format: "sint16" },
                { shaderLocation: 7, offset: 12, format: "unorm16" },
                { shaderLocation: 8, offset: 14, format: "snorm16" },
                { shaderLocation: 9, offset: 16, format: "float16" },
              ],
            },
          ],
        },
        fragment: { module, targets: [{ format: "rgba8unorm" }] },
      });
      return pipeline.label;
    });
    expect(result).toBe("");
  });
});
