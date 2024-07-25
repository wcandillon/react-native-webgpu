import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { WebGPUViewRef } from "react-native-webgpu";
import { gpu, WebGPUView } from "react-native-webgpu";
import { mat4, vec3 } from "wgpu-matrix";

import {
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexCount,
  cubeVertexSize,
} from "../components/cube";

import { basicVertWGSL, vertexPositionColorWGSL } from "./Shaders";

export function Cube() {
  const ref = useRef<WebGPUViewRef>(null);

  async function demo() {
    const adapter = await gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = gpu.getPreferredCanvasFormat();

    const context = ref.current!.getContext("webgpu")!;
    const { canvas } = context;
    if (!context) {
      throw new Error("No context");
    }
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

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
      size: [canvas.width, canvas.height],
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

    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        {
          view: undefined, // Assigned later

          clearValue: [0.5, 0.5, 0.5, 1.0],
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

    const aspect = canvas.width / canvas.height;
    const projectionMatrix = mat4.perspective(
      (2 * Math.PI) / 5,
      aspect,
      1,
      100.0,
    );
    const modelViewProjectionMatrix = mat4.create();

    function getTransformationMatrix() {
      const viewMatrix = mat4.identity();
      mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
      const now = Date.now() / 1000;
      mat4.rotate(
        viewMatrix,
        vec3.fromValues(Math.sin(now), Math.cos(now), 0),
        1,
        viewMatrix,
      );

      mat4.multiply(projectionMatrix, viewMatrix, modelViewProjectionMatrix);

      return modelViewProjectionMatrix;
    }

    function frame() {
      const transformationMatrix = getTransformationMatrix();
      device.queue.writeBuffer(
        uniformBuffer,
        0,
        transformationMatrix.buffer,
        transformationMatrix.byteOffset,
        transformationMatrix.byteLength,
      );
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

      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
  }

  useEffect(() => {
    demo();
  }, [ref]);

  return (
    <View style={style.container}>
      <WebGPUView ref={ref} style={style.webgpu} />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "red",
  },
  webgpu: {
    flex: 1,
  },
});
