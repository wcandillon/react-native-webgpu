/* eslint-disable @typescript-eslint/ban-ts-comment */
import React from "react";
import { StyleSheet, View } from "react-native";
import type { RNCanvasContext } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import type { SharedValue } from "react-native-reanimated";
import { useAnimatedReaction } from "react-native-reanimated";

import {
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexCount,
  cubeVertexSize,
} from "../components/cube";
import {
  useAnimatedContext,
  useAnimatedRenderer,
  useClock,
} from "../components/Animations";

import { basicVertWGSL, vertexPositionColorWGSL } from "./Shaders";

const init = (device: GPUDevice, context: RNCanvasContext) => {
  const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  // Create a vertex buffer from the cube data.
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
      module: device.createShaderModule({
        code: basicVertWGSL,
      }),
      buffers: [
        {
          arrayStride: cubeVertexSize,
          attributes: [
            {
              // position
              shaderLocation: 0,
              offset: cubePositionOffset,
              format: "float32x4",
            },
            {
              // uv
              shaderLocation: 1,
              offset: cubeUVOffset,
              format: "float32x2",
            },
          ],
        },
      ],
    },
    fragment: {
      module: device.createShaderModule({
        code: vertexPositionColorWGSL,
      }),
      targets: [
        {
          format: presentationFormat,
        },
      ],
    },
    primitive: {
      topology: "triangle-list",

      // Backface culling since the cube is solid piece of geometry.
      // Faces pointing away from the camera will be occluded by faces
      // pointing toward the camera.
      cullMode: "back",
    },

    // Enable depth testing so that the fragment closest to the camera
    // is rendered in front.
    depthStencil: {
      depthWriteEnabled: true,
      depthCompare: "less",
      format: "depth24plus",
    },
  });

  const depthTexture = device.createTexture({
    size: [context.canvas.width, context.canvas.height],
    format: "depth24plus",
    usage: GPUTextureUsage.RENDER_ATTACHMENT,
  });

  const uniformBufferSize = 4 * 16; // 4x4 matrix
  const uniformBuffer = device.createBuffer({
    size: uniformBufferSize,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  });

  const uniformBindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      {
        binding: 0,
        resource: {
          buffer: uniformBuffer,
        },
      },
    ],
  });
  return {
    device,
    context,
    depthTexture,
    uniformBindGroup,
    pipeline,
    verticesBuffer,
    uniformBuffer,
  };
};

type Values = {
  clock: SharedValue<number>;
};

const frame = (ctx: ReturnType<typeof init>, { clock }: Values) => {
  "worklet";
  const {
    device,
    pipeline,
    uniformBindGroup,
    verticesBuffer,
    context,
    depthTexture,
    uniformBuffer,
  } = ctx;
  const renderPassDescriptor: GPURenderPassDescriptor = {
    // @ts-expect-error
    colorAttachments: [
      {
        view: undefined, // Assigned later
        clearValue: [0, 0, 0, 0],
        loadOp: "clear",
        storeOp: "store",
      },
    ],
    depthStencilAttachment: {
      view: depthTexture.createView(),

      depthClearValue: 1.0,
      depthLoadOp: "clear",
      depthStoreOp: "store",
    },
  };

  const transformationMatrix = new Float32Array([
    2.5817277431488037, -0.12328082323074341, -0.16898392140865326,
    -0.16729408502578735, -0.2355215847492218, 0.7686712145805359,
    -0.8330033421516418, -0.8246733546257019, -0.43990078568458557,
    -1.1350654363632202, -0.5457598567008972, -0.5403022766113281, 0, 0,
    3.0303030014038086, 4,
  ]);
  device.queue.writeBuffer(
    uniformBuffer,
    0,
    transformationMatrix.buffer,
    transformationMatrix.byteOffset,
    transformationMatrix.byteLength,
  );
  // @ts-expect-error
  renderPassDescriptor.colorAttachments[0].view = context
    .getCurrentTexture()
    .createView();

  const commandEncoder = device.createCommandEncoder();
  const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
  passEncoder.setPipeline(pipeline);
  passEncoder.setBindGroup(0, uniformBindGroup);
  passEncoder.setVertexBuffer(0, verticesBuffer);
  passEncoder.draw(cubeVertexCount);
  passEncoder.end();
  device.queue.submit([commandEncoder.finish()]);
  context.present();
};

export function Cube() {
  const clock = useClock();
  const { ref } = useAnimatedRenderer(init, frame, { clock });
  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} transparent />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "rgb(90, 180, 255)",
  },
  webgpu: {
    flex: 1,
  },
});
