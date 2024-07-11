import React, { useEffect } from "react";
import { View } from "react-native";
import { gpu } from "react-native-webgpu";

import { cubeVertexArray } from "./components/cube";

const demo = async () => {
  const adapter = await gpu.requestAdapter(undefined);
  if (!adapter) {
    throw new Error("No adapter");
  }
  const device = await adapter.requestDevice();
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
  console.log("before read mapping");
  await verticesBuffer.mapAsync(GPUMapMode.READ);
  console.log("after read mapping");
  console.log(new Float32Array(verticesBuffer.getMappedRange()));
  verticesBuffer.unmap();
};

const loop = () => {
  console.log("tick");
  requestAnimationFrame(loop);
};

export const Buffers = () => {
  useEffect(() => {
    loop();
    demo();
  }, []);
  return <View />;
};
