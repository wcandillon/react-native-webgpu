import React from "react";
import { StyleSheet, View } from "react-native";
import { Canvas } from "react-native-wgpu";

import { useWebGPU } from "../components/useWebGPU";

import { shaderCode } from "./shaders";

const rand = (min?: number, max?: number) => {
  if (min === undefined) {
    min = 0;
    max = 1;
  } else if (max === undefined) {
    max = min;
    min = 0;
  }
  return min + Math.random() * (max - min);
};

function createCircleVertices({
  radius = 1,
  numSubdivisions = 24,
  innerRadius = 0,
  startAngle = 0,
  endAngle = Math.PI * 2,
} = {}) {
  const numVertices = numSubdivisions * 3 * 2;
  const vertexData = new Float32Array(numSubdivisions * 2 * 3 * 2);

  let offset = 0;
  const addVertex = (x: number, y: number) => {
    vertexData[offset++] = x;
    vertexData[offset++] = y;
  };

  for (let i = 0; i < numSubdivisions; ++i) {
    const angle1 =
      startAngle + ((i + 0) * (endAngle - startAngle)) / numSubdivisions;
    const angle2 =
      startAngle + ((i + 1) * (endAngle - startAngle)) / numSubdivisions;

    const c1 = Math.cos(angle1);
    const s1 = Math.sin(angle1);
    const c2 = Math.cos(angle2);
    const s2 = Math.sin(angle2);

    // first triangle
    addVertex(c1 * radius, s1 * radius);
    addVertex(c2 * radius, s2 * radius);
    addVertex(c1 * innerRadius, s1 * innerRadius);

    // second triangle
    addVertex(c1 * innerRadius, s1 * innerRadius);
    addVertex(c2 * radius, s2 * radius);
    addVertex(c2 * innerRadius, s2 * innerRadius);
  }

  return {
    vertexData,
    numVertices,
  };
}

export function StorageBufferVertices() {
  const ref = useWebGPU(({ context, device, presentationFormat, canvas }) => {
    const module = device.createShaderModule({
      code: shaderCode,
    });

    const pipeline = device.createRenderPipeline({
      label: "storage buffer vertices",
      layout: "auto",
      vertex: {
        module,
      },
      fragment: {
        module,
        targets: [{ format: presentationFormat }],
      },
    });

    const kNumObjects = 100;
    const objectInfos: { scale: number }[] = [];

    // create 2 storage buffers
    const staticUnitSize =
      4 * 4 + // color is 4 32bit floats (4bytes each)
      2 * 4 + // offset is 2 32bit floats (4bytes each)
      2 * 4; // padding
    const changingUnitSize = 2 * 4; // scale is 2 32bit floats (4bytes each)
    const staticStorageBufferSize = staticUnitSize * kNumObjects;
    const changingStorageBufferSize = changingUnitSize * kNumObjects;

    const staticStorageBuffer = device.createBuffer({
      label: "static storage for objects",
      size: staticStorageBufferSize,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });

    const changingStorageBuffer = device.createBuffer({
      label: "changing storage for objects",
      size: changingStorageBufferSize,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });

    // offsets to the various uniform values in float32 indices
    const kColorOffset = 0;
    const kOffsetOffset = 4;
    const kScaleOffset = 0;

    const staticStorageValues = new Float32Array(staticStorageBufferSize / 4);
    for (let i = 0; i < kNumObjects; ++i) {
      const staticOffset = i * (staticUnitSize / 4);

      // These are only set once so set them now
      staticStorageValues.set(
        [rand(), rand(), rand(), 1],
        staticOffset + kColorOffset,
      ); // set the color
      staticStorageValues.set(
        [rand(-0.9, 0.9), rand(-0.9, 0.9)],
        staticOffset + kOffsetOffset,
      ); // set the offset

      objectInfos.push({
        scale: rand(0.2, 0.5),
      });
    }
    device.queue.writeBuffer(staticStorageBuffer, 0, staticStorageValues);

    // a typed array we can use to update the changingStorageBuffer
    const storageValues = new Float32Array(changingStorageBufferSize / 4);

    // setup a storage buffer with vertex data
    const { vertexData, numVertices } = createCircleVertices({
      radius: 0.5,
      innerRadius: 0.25,
    });
    const vertexStorageBuffer = device.createBuffer({
      label: "storage buffer vertices",
      size: vertexData.byteLength,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });
    device.queue.writeBuffer(vertexStorageBuffer, 0, vertexData);

    const bindGroup = device.createBindGroup({
      label: "bind group for objects",
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: staticStorageBuffer },
        { binding: 1, resource: changingStorageBuffer },
        { binding: 2, resource: vertexStorageBuffer },
      ],
    });

    const renderPassDescriptor: GPURenderPassDescriptor = {
      label: "our basic canvas renderPass",
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [0.3, 0.3, 0.3, 1],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    };

    // Set the uniform values in our JavaScript side Float32Array
    const aspect = canvas.width / canvas.height;

    // set the scales for each object
    objectInfos.forEach(({ scale }, ndx) => {
      const offset = ndx * (changingUnitSize / 4);
      storageValues.set([scale / aspect, scale], offset + kScaleOffset);
    });
    // upload all scales at once
    device.queue.writeBuffer(changingStorageBuffer, 0, storageValues);

    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass(renderPassDescriptor);
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.draw(numVertices, kNumObjects);
    pass.end();

    const commandBuffer = encoder.finish();
    device.queue.submit([commandBuffer]);
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (context as any).present();
  });

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
});
