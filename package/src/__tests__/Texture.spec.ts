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
  // it("create texture", async () => {
  //   const result = await client.eval(({ device }) => {
  //     const textureSize = 512;
  //     const texture = device.createTexture({
  //       size: [textureSize, textureSize],
  //       format: "rgba8unorm",
  //       usage:
  //         GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
  //       label: "texture",
  //     });
  //     return texture.label;
  //   });
  //   expect(result).toBe("texture");
  // });
});
