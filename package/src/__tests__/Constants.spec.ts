import { client } from "./setup";

describe("WebGPUConstants", () => {
  it("GPUBufferUsage", async () => {
    const result = await client.eval(() => {
      return GPUBufferUsage;
    });
    expect(result).toEqual({
      MAP_READ: 1,
      MAP_WRITE: 2,
      COPY_SRC: 4,
      COPY_DST: 8,
      INDEX: 16,
      VERTEX: 32,
      UNIFORM: 64,
      STORAGE: 128,
      INDIRECT: 256,
      QUERY_RESOLVE: 512,
    });
  });
  it("GPUColorWrite", async () => {
    const result = await client.eval(() => {
      return GPUColorWrite;
    });
    expect(result).toEqual({
      RED: 1,
      GREEN: 2,
      BLUE: 4,
      ALPHA: 8,
      ALL: 15,
    });
  });
  it("GPUMapMode", async () => {
    const result = await client.eval(() => {
      return GPUMapMode;
    });
    expect(result).toEqual({
      READ: 1,
      WRITE: 2,
    });
  });
  it("GPUShaderStage", async () => {
    const result = await client.eval(() => {
      return GPUShaderStage;
    });
    expect(result).toEqual({
      VERTEX: 1,
      FRAGMENT: 2,
      COMPUTE: 4,
    });
  });
  it("GPUTextureUsage", async () => {
    const result = await client.eval(() => {
      return GPUTextureUsage;
    });
    expect(result).toEqual({
      COPY_SRC: 1,
      COPY_DST: 2,
      TEXTURE_BINDING: 4,
      STORAGE_BINDING: 8,
      RENDER_ATTACHMENT: 16,
    });
  });
  it("GPUAdapter", async () => {
    const result = await client.eval(({ adapter, device, gpu }) => {
      return [
        gpu instanceof GPU,
        adapter instanceof GPUAdapter,
        device instanceof GPUDevice,
        adapter instanceof GPUDevice,
        device instanceof GPUAdapter,
      ];
    });
    expect(result).toEqual([true, true, true, false, false]);
  });
  it("instanceof", async () => {
    const result = await client.eval(
      ({ device, shaders: { triangleVertWGSL } }) => {
        const buffer = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
        });
        const module = device.createShaderModule({
          code: triangleVertWGSL,
        });
        return [buffer instanceof GPUBuffer, module instanceof GPUShaderModule];
      },
    );
    expect(result).toEqual([true, true]);
  });
});
