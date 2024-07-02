import React, { useEffect } from "react";
import { View } from "react-native";
import { gpu } from "react-native-webgpu";

import { cubeVertexArray } from "./components/cube";

const demo = async () => {
  const adapter = await gpu.requestAdapter();
  if (!adapter) {
    throw new Error("No adapter");
  }
  const device = await adapter.requestDevice();
  const verticesBuffer = device.createBuffer({
    size: cubeVertexArray.byteLength,
    usage: GPUBufferUsage.VERTEX,
    mappedAtCreation: true,
  });
  new Float32Array(
    verticesBuffer.getMappedRange(0, cubeVertexArray.byteLength),
  ).set(cubeVertexArray);
  verticesBuffer.unmap();
  console.log("DONE!");
};

export const Buffers = () => {
  useEffect(() => {
    demo();
  }, []);
  return <View />;
};
