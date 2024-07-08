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
    expect(result).toEqual(["label", "verticesBuffer"]);
  });
  // it("upload data", async () => {
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
  //     // verticesBuffer.unmap();
  //     return !!verticesBuffer;
  //   });
  //   expect(result).toBe(true);
  // });
});
