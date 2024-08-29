import { client } from "./setup";

describe("Buffer", () => {
  it("Array Buffer (1)", async () => {
    const result = await client.eval(() => {
      return Array.from(new Uint8Array([1.0, 2.0, 3.0, 4.0]));
    });
    expect(result.slice(0, 4)).toEqual([1, 2, 3, 4]);
  });
  it("Array Buffer (2)", async () => {
    const result = await client.eval(({ device }) => {
      const data = new Float32Array([1.0, 2.0, 3.0, 4.0]);
      const bufferSize = data.byteLength; // 4 32-bit floats
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

      // Write data to the source buffer
      device.queue.writeBuffer(sourceBuffer, 0, data);

      // Copy data from source buffer to read buffer
      const encoder = device.createCommandEncoder();
      encoder.copyBufferToBuffer(sourceBuffer, 0, readBuffer, 0, bufferSize);
      device.queue.submit([encoder.finish()]);
      return device.queue.onSubmittedWorkDone().then(() => {
        // Map the read buffer for reading
        return readBuffer.mapAsync(GPUMapMode.READ).then(() => {
          const readData = new Float32Array(readBuffer.getMappedRange());
          return Array.from(readData);
        });
      });
    });
    expect(result).toEqual([1, 2, 3, 4]);
  });
  it("Array Buffer (3)", async () => {
    const result = await client.eval(({ device }) => {
      const data = new Float32Array([1.0, 2.0, 3.0, 4.0]);
      const bufferSize = data.byteLength; // 4 32-bit floats
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

      // Write data to the source buffer
      device.queue.writeBuffer(sourceBuffer, 0, data);

      // Copy data from source buffer to read buffer
      const encoder = device.createCommandEncoder();
      encoder.copyBufferToBuffer(sourceBuffer, 0, readBuffer, 0, bufferSize);
      device.queue.submit([encoder.finish()]);
      // Map the read buffer for reading
      return readBuffer.mapAsync(GPUMapMode.READ).then(() => {
        const readData = new Float32Array(readBuffer.getMappedRange());
        return Array.from(readData);
      });
    });
    expect(result).toEqual([1, 2, 3, 4]);
  });
});
