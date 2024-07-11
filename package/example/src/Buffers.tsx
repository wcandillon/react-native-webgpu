import React, { useEffect } from "react";
import { View } from "react-native";
import { gpu } from "react-native-webgpu";

// TODO: have similar validation error: https://capture.dropbox.com/LX893uO7O7SsLLZE (    usage: GPUBufferUsage.VERTEX)

const demo = async () => {
  const adapter = await gpu.requestAdapter();
  if (!adapter) {
    console.error("Couldn't request WebGPU adapter.");
    return;
  }

  const device = await adapter.requestDevice();

  // Create a buffer for initial data and copying
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
  await readBuffer.mapAsync(GPUMapMode.READ);
  const readData = new Float32Array(readBuffer.getMappedRange());
  console.log("Data read from buffer:", readData);
  readBuffer.unmap();

  // Create a buffer for writing
  const writeBuffer = device.createBuffer({
    size: bufferSize,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.MAP_WRITE,
  });

  // Map the write buffer for writing
  await writeBuffer.mapAsync(GPUMapMode.WRITE);
  const writeData = new Float32Array(writeBuffer.getMappedRange());
  writeData.set([5.0, 6.0, 7.0, 8.0]);
  console.log("Data written to buffer:", writeData);
  writeBuffer.unmap();

  // Copy data from write buffer to source buffer
  const encoder2 = device.createCommandEncoder();
  encoder2.copyBufferToBuffer(writeBuffer, 0, sourceBuffer, 0, bufferSize);
  device.queue.submit([encoder2.finish()]);

  // Copy data from source buffer to read buffer
  const encoder3 = device.createCommandEncoder();
  encoder3.copyBufferToBuffer(sourceBuffer, 0, readBuffer, 0, bufferSize);
  device.queue.submit([encoder3.finish()]);

  // Map the read buffer for final reading
  await readBuffer.mapAsync(GPUMapMode.READ);
  const finalReadData = new Float32Array(readBuffer.getMappedRange());
  console.log("Final data read from buffer:", finalReadData);
  readBuffer.unmap();

  console.log("WebGPU operations completed successfully.");
};

const loop = () => {
  //console.log("tick");
  requestAnimationFrame(loop);
};

export const Buffers = () => {
  useEffect(() => {
    loop();
    demo();
  }, []);
  return <View />;
};
