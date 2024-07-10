import React, { useEffect } from "react";
import { View } from "react-native";
import { gpu } from "react-native-webgpu";

import { cubeVertexArray } from "./components/cube";

const demo = async () => {
  console.log(gpu.getPreferredCanvasFormat());
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
  // new Float32Array(
  //   verticesBuffer.getMappedRange(0, cubeVertexArray.byteLength),
  // ).set(cubeVertexArray);
  // verticesBuffer.unmap();
  console.log(verticesBuffer.label);
  console.log("DONE!" + !!verticesBuffer);
};

export const Buffers = () => {
  useEffect(() => {
    demo();
  }, []);
  return <View />;
};
