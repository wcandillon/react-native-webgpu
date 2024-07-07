import { client } from "./setup";

describe("Buffer", () => {
  it("upload data", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const verticesBuffer = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
      });
      return !!verticesBuffer;

      // new Float32Array(
      //   verticesBuffer.getMappedRange(0, cubeVertexArray.byteLength),
      // ).set(cubeVertexArray);
      // verticesBuffer.unmap();
    });
    expect(result).toBe(true);
  });
});
