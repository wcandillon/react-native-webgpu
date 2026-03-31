import React, { useMemo } from "react";
import { FlatList, StyleSheet, View } from "react-native";
import { GPUDeviceProvider, useMainDevice } from "react-native-wgpu";

import {
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexSize,
} from "../components/cube";

import { CubeItem } from "./CubeItem";
import type { SharedResources } from "./CubeItem";
import { tintedFragWGSL, tintedVertWGSL } from "./Shaders";

const NUM_ITEMS = 50;
const ITEM_HEIGHT = 250;

const data = Array.from({ length: NUM_ITEMS }, (_, i) => ({ id: i }));

function MultiContextList() {
  const { device } = useMainDevice();

  const sharedResources = useMemo<SharedResources | null>(() => {
    if (!device) {
      return null;
    }

    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

    const verticesBuffer = device.createBuffer({
      size: cubeVertexArray.byteLength,
      usage: GPUBufferUsage.VERTEX,
      mappedAtCreation: true,
    });
    new Float32Array(verticesBuffer.getMappedRange()).set(cubeVertexArray);
    verticesBuffer.unmap();

    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: {
        module: device.createShaderModule({ code: tintedVertWGSL }),
        buffers: [
          {
            arrayStride: cubeVertexSize,
            attributes: [
              {
                shaderLocation: 0,
                offset: cubePositionOffset,
                format: "float32x4" as GPUVertexFormat,
              },
              {
                shaderLocation: 1,
                offset: cubeUVOffset,
                format: "float32x2" as GPUVertexFormat,
              },
            ],
          },
        ],
      },
      fragment: {
        module: device.createShaderModule({ code: tintedFragWGSL }),
        targets: [{ format: presentationFormat }],
      },
      primitive: {
        topology: "triangle-list",
        cullMode: "back",
      },
      depthStencil: {
        depthWriteEnabled: true,
        depthCompare: "less",
        format: "depth24plus",
      },
    });

    return { pipeline, verticesBuffer, presentationFormat };
  }, [device]);

  if (!sharedResources) {
    return null;
  }

  return (
    <FlatList
      data={data}
      keyExtractor={(item) => String(item.id)}
      renderItem={({ item }) => (
        <CubeItem
          index={item.id}
          itemHeight={ITEM_HEIGHT}
          sharedResources={sharedResources}
          device={device!}
        />
      )}
      getItemLayout={(_, index) => ({
        length: ITEM_HEIGHT,
        offset: ITEM_HEIGHT * index,
        index,
      })}
      windowSize={5}
      maxToRenderPerBatch={3}
      initialNumToRender={6}
    />
  );
}

export function MultiContext() {
  return (
    <View style={styles.container}>
      <GPUDeviceProvider>
        <MultiContextList />
      </GPUDeviceProvider>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#111",
  },
});
