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
  console.log(verticesBuffer);
  // new Float32Array(verticesBuffer.getMappedRange()).set(cubeVertexArray);
  // verticesBuffer.unmap();
};

export const Buffers = () => {
  useEffect(() => {
    demo();
  }, []);
  return <View />;
};
