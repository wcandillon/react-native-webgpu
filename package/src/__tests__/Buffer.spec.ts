import exp from "constants";

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
  it("metadata (1)", async () => {
    const result = await client.eval(({ device, cubeVertexArray }) => {
      const buffer = device.createBuffer({
        size: cubeVertexArray.byteLength,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: false,
      });
      return [buffer.label, buffer.size, buffer.usage, buffer.mapState];
    });
    expect(result).toEqual(["", 1440, 32, "unmapped"]);
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
  it("writes into a buffer (1)", async () => {
    const result = await client.eval(({ device }) => {
      const bufferSize = 4 * 4; // 4 32-bit floats
      const sourceBuffer = device.createBuffer({
        size: bufferSize,
        usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
      });

      // Create data to upload
      const data = new Float32Array([1.0, 2.0, 3.0, 4.0]);

      // Write data to the source buffer
      device.queue.writeBuffer(sourceBuffer, 0, data);
      return true;
    });
    expect(result).toBe(true);
  });
  it("read/write buffer (1)", async () => {
    const result = await client.eval(({ device }) => {
      const data = new Uint32Array([1.0, 2.0, 3.0, 4.0]);
      // Create a GPU buffer and store data
      const gpuBuffer = device.createBuffer({
        size: data.byteLength,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
        mappedAtCreation: false,
      });

      // Copy data to the GPU buffer
      device.queue.writeBuffer(gpuBuffer, 0, data);

      return gpuBuffer.mapAsync(GPUMapMode.READ).then(() => {
        const arrayBuffer = gpuBuffer.getMappedRange();
        const readData = new Uint32Array(arrayBuffer);
        const r = Array.from(readData);
        gpuBuffer.unmap();
        return r;
      });
    });
    expect(result).toEqual([1, 2, 3, 4]);
  });
  // it("read/write buffer (1)", async () => {
  //   const result = await client.eval(({ device }) => {
  //     const data = new Uint32Array([1.0, 2.0, 3.0, 4.0]);
  //     // Create a GPU buffer and store data
  //     const gpuBuffer = device.createBuffer({
  //       size: data.byteLength,
  //       usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
  //       mappedAtCreation: false,
  //     });

  //     // Copy data to the GPU buffer
  //     device.queue.writeBuffer(gpuBuffer, 0, data.buffer, 0, data.byteLength);

  //     return gpuBuffer
  //       .mapAsync(GPUMapMode.READ, 0, data.byteLength)
  //       .then(() => {
  //         const arrayBuffer = gpuBuffer.getMappedRange(0, data.byteLength);
  //         const readData = new Float32Array(arrayBuffer);
  //         const r = Array.from(readData);
  //         gpuBuffer.unmap();
  //         return r;
  //       });
  //   });
  //   expect(result).toEqual([1, 2, 3, 4]);
  // });
  it("writes into a buffer (2)", async () => {
    const result = await client.eval(({ device }) => {
      const bufferSize = 4 * 4; // 4 32-bit floats
      const sourceBuffer = device.createBuffer({
        size: bufferSize,
        usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
      });

      // Create a buffer for reading
      const readBuffer = device.createBuffer({
        size: bufferSize,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });

      // Create data to upload
      const data = new Float32Array([1.0, 2.0, 3.0, 4.0]);

      // Write data to the source buffer
      device.queue.writeBuffer(sourceBuffer, 0, data);

      // Copy data from source buffer to read buffer
      const encoder = device.createCommandEncoder();
      encoder.copyBufferToBuffer(sourceBuffer, 0, readBuffer, 0, bufferSize);
      device.queue.submit([encoder.finish()]);
      // Map the read buffer for reading
      return readBuffer.mapAsync(GPUMapMode.READ).then(() => {
        const readData = new Float32Array(readBuffer.getMappedRange());
        const res = Array.from(readData);
        readBuffer.unmap();
        return res;
      });

      // // Create a buffer for writing
      // const writeBuffer = device.createBuffer({
      //   size: bufferSize,
      //   usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.MAP_WRITE,
      // });

      // // Map the write buffer for writing
      // await writeBuffer.mapAsync(GPUMapMode.WRITE);
      // const writeData = new Float32Array(writeBuffer.getMappedRange());
      // writeData.set([5.0, 6.0, 7.0, 8.0]);
      // console.log("Data written to buffer:", writeData);
      // writeBuffer.unmap();

      // // Copy data from write buffer to source buffer
      // const encoder2 = device.createCommandEncoder();
      // encoder2.copyBufferToBuffer(writeBuffer, 0, sourceBuffer, 0, bufferSize);
      // device.queue.submit([encoder2.finish()]);

      // // Copy data from source buffer to read buffer
      // const encoder3 = device.createCommandEncoder();
      // encoder3.copyBufferToBuffer(sourceBuffer, 0, readBuffer, 0, bufferSize);
      // device.queue.submit([encoder3.finish()]);

      // // Map the read buffer for final reading
      // await readBuffer.mapAsync(GPUMapMode.READ);
      // const finalReadData = new Float32Array(readBuffer.getMappedRange());
      // console.log("Final data read from buffer:", finalReadData);
      // readBuffer.unmap();

      // console.log("WebGPU operations completed successfully.");
    });
    expect(result).toEqual([1, 2, 3, 4]);
  });
});
