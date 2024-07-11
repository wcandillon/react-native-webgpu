import { client } from "./setup";

describe("Buffer", () => {
  it("Label", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const buffer1 = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
      });
      const buffer2 = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
        label: "verticesBuffer",
      });
      return [buffer1.label, buffer2.label];
    });
    expect(result).toEqual(["", "verticesBuffer"]);
  });
  it("metadata", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const buffer = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
      });
      return [buffer.label, buffer.size, buffer.usage, buffer.mapState];
    });
    expect(result).toEqual(["", 1440, 32, "mapped"]);
  });
  it("upload data (1)", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const verticesBuffer = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
        label: "verticesBuffer",
      });
      new Float32Array(verticesBuffer.getMappedRange()).set(cubeVertexArray);
      verticesBuffer.unmap();
      return !!verticesBuffer;
    });
    expect(result).toBe(true);
  });

  it("upload data (2)", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const verticesBuffer = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
        label: "verticesBuffer",
      });
      new Float32Array(
        verticesBuffer.getMappedRange(0, cubeVertexArray.byteLength),
      ).set(cubeVertexArray);
      verticesBuffer.unmap();
      return !!verticesBuffer;
    });
    expect(result).toBe(true);
  });
  // This should throw similar errors than chrome
  // it("upload data & read data (2)", async () => {
  //   const result = await client.eval(({ device, cubeVertexArray }) => {
  //     const verticesBuffer = device.createBuffer({
  //       size: cubeVertexArray.byteLength,
  //       usage: GPUBufferUsage.VERTEX,
  //       mappedAtCreation: true,
  //       label: "verticesBuffer",
  //     });
  //     new Float32Array(
  //       verticesBuffer.getMappedRange(0, cubeVertexArray.byteLength),
  //     ).set(cubeVertexArray);
  //     verticesBuffer.unmap();
  //     return verticesBuffer.mapAsync(GPUMapMode.READ).then(() => {
  //       const r = new Float32Array(verticesBuffer.getMappedRange());
  //       return r;
  //     });
  //   });
  //   expect(result).toBe([0, 0, 0]);
  // });
});
