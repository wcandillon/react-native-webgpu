import { client } from "./setup";

describe("Texture", () => {
  it("Check usage", async () => {
    const result = await client.eval(() => {
      return (
        GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING
      );
    });
    expect(result).toBe(20);
  });
  it("create texture (1)", async () => {
    const result = await client.eval(({ device }) => {
      const textureSize = 512;
      const texture = device.createTexture({
        size: [textureSize, textureSize],
        format: "rgba8unorm",
        usage:
          GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
        label: "texture",
      });
      return [
        texture.label,
        texture.width,
        texture.height,
        texture.depthOrArrayLayers,
        texture.mipLevelCount,
        texture.sampleCount,
        texture.dimension,
        texture.format,
        texture.usage,
      ];
    });
    expect(result).toEqual([
      "texture",
      512,
      512,
      1,
      1,
      1,
      "2d",
      "rgba8unorm",
      20,
    ]);
  });
});
