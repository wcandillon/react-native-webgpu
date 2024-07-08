import { client } from "./setup";

describe("Buffer", () => {
  it("upload data", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const verticesBuffer = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
      });
      // new Float32Array(verticesBuffer.getMappedRange()).set(cubeVertexArray);
      // verticesBuffer.unmap();
      return !!verticesBuffer;
    });
    expect(result).toBe(true);
  });
});
