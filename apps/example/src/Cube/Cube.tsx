/* eslint-disable @typescript-eslint/ban-ts-comment */
import React, { useCallback } from "react";
import { StyleSheet, View } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import type { FrameInfo } from "react-native-reanimated";
import {
  useAnimatedReaction,
  useFrameCallback,
  useSharedValue,
} from "react-native-reanimated";

import {
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexCount,
  cubeVertexSize,
} from "../components/cube";

import { basicVertWGSL, vertexPositionColorWGSL } from "./Shaders";

export const useClock = () => {
  const clock = useSharedValue(0);
  const callback = useCallback(
    (info: FrameInfo) => {
      "worklet";
      clock.value = info.timeSinceFirstFrame;
    },
    [clock],
  );
  useFrameCallback(callback);
  return clock;
};

interface WebGPUCtx {
  device: GPUDevice;
  context: GPUCanvasContext;
  depthTexture: GPUTexture;
  uniformBindGroup: GPUBindGroup;
  pipeline: GPURenderPipeline;
  verticesBuffer: GPUBuffer;
  uniformBuffer: GPUBuffer;
}

export function Cube() {
  const clock = useClock();
  const ctx = useSharedValue<WebGPUCtx | null>(null);
  const ref = useCanvasEffect(async () => {
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    ref.current?.getContext("webgpu")?.configure({
      device,
      format: presentationFormat,
      alphaMode: "opaque",
    });
    const canvas = ref.current;
    if (!canvas) {
      throw new Error("No canvas available");
    }
    const context = canvas.getContext("webgpu");
    if (!context) {
      throw new Error("No WebGPU context available");
    }
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
    ctx.value = {
      device,
      context,
      depthTexture,
      uniformBindGroup,
      pipeline,
      verticesBuffer,
      uniformBuffer,
    };
  });

  useAnimatedReaction(
    () => clock.value,
    (time) => {
      if (!ctx.value) {
        return;
      }
      const {
        device,
        pipeline,
        uniformBindGroup,
        verticesBuffer,
        context,
        depthTexture,
        uniformBuffer,
      } = ctx.value;
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

      const aspect = context.canvas.width / context.canvas.height;
      const transformationMatrix = Float32Array.of(
          1, 0, 0, 0,
          0, 1, 0, 0,
          0, 0, 1, 0,
          0, 0, 0, 1,
      );
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
      console.log("Time: ", time);
    },
  );
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
