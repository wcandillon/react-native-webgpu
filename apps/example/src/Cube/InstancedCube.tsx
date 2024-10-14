import React from "react";
import { StyleSheet, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import type { Mat4 } from "wgpu-matrix";
import { mat4, vec3 } from "wgpu-matrix";

import {
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexCount,
  cubeVertexSize,
} from "../components/cube";
import { useWebGPU } from "../components/useWebGPU";

import { instancedVertWGSL, vertexPositionColorWGSL } from "./Shaders";

export const InstancedCube = () => {
  const ref = useWebGPU(({ device, presentationFormat, canvas, context }) => {
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
          code: instancedVertWGSL,
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

    const xCount = 4;
    const yCount = 4;
    const numInstances = xCount * yCount;
    const matrixFloatCount = 16; // 4x4 matrix
    const matrixSize = 4 * matrixFloatCount;
    const uniformBufferSize = numInstances * matrixSize;

    // Allocate a buffer large enough to hold transforms for every
    // instance.
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

    const aspect = canvas.width / canvas.height;
    const projectionMatrix = mat4.perspective(
      (2 * Math.PI) / 5,
      aspect,
      1,
      100.0,
    );

    const modelMatrices = new Array<Mat4>(numInstances);
    const mvpMatricesData = new Float32Array(matrixFloatCount * numInstances);

    const step = 4.0;

    // Initialize the matrix data for every instance.
    let m = 0;
    for (let x = 0; x < xCount; x++) {
      for (let y = 0; y < yCount; y++) {
        modelMatrices[m] = mat4.translation(
          vec3.fromValues(
            step * (x - xCount / 2 + 0.5),
            step * (y - yCount / 2 + 0.5),
            0,
          ),
        );
        m++;
      }
    }

    const viewMatrix = mat4.translation(vec3.fromValues(0, 0, -12));

    const tmpMat4 = mat4.create();

    // Update the transformation matrix data for each instance.
    function updateTransformationMatrix() {
      const now = Date.now() / 1000;

      // eslint-disable-next-line @typescript-eslint/no-shadow
      let m = 0,
        i = 0;
      for (let x = 0; x < xCount; x++) {
        for (let y = 0; y < yCount; y++) {
          mat4.rotate(
            modelMatrices[i],
            vec3.fromValues(
              Math.sin((x + 0.5) * now),
              Math.cos((y + 0.5) * now),
              0,
            ),
            1,
            tmpMat4,
          );

          mat4.multiply(viewMatrix, tmpMat4, tmpMat4);
          mat4.multiply(projectionMatrix, tmpMat4, tmpMat4);

          mvpMatricesData.set(tmpMat4, m);

          i++;
          m += matrixFloatCount;
        }
      }
    }

    const renderPassDescriptor: GPURenderPassDescriptor = {
      // eslint-disable-next-line @typescript-eslint/ban-ts-comment
      // @ts-expect-error
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

    function frame() {
      // Update the matrix data.
      updateTransformationMatrix();
      device.queue.writeBuffer(
        uniformBuffer,
        0,
        mvpMatricesData.buffer,
        mvpMatricesData.byteOffset,
        mvpMatricesData.byteLength,
      );

      // eslint-disable-next-line @typescript-eslint/ban-ts-comment
      // @ts-expect-error
      renderPassDescriptor.colorAttachments[0].view = context
        .getCurrentTexture()
        .createView();

      const commandEncoder = device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.setBindGroup(0, uniformBindGroup);
      passEncoder.setVertexBuffer(0, verticesBuffer);
      passEncoder.draw(cubeVertexCount, numInstances, 0, 0);
      passEncoder.end();
      device.queue.submit([commandEncoder.finish()]);
    }
    return frame;
  });

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
    </View>
  );
};

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
});
