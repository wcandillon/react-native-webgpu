"use client";

import { useCallback } from "react";
import { PixelRatio, StyleSheet, View } from "react-native";
import { Canvas } from "react-native-webgpu";
import { mat4, vec3 } from "wgpu-matrix";

import {
  basicVertWGSL,
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexCount,
  cubeVertexSize,
  vertexPositionColorWGSL,
} from "./shaders/cube";
import { useWebGPU, type SceneProps } from "./useWebGPU";

export function RotatingCubeDemo() {
  const scene = useCallback(({ context, device, presentationFormat, canvas }: SceneProps) => {
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
          module: device.createShaderModule({ code: basicVertWGSL }),
          buffers: [
            {
              arrayStride: cubeVertexSize,
              attributes: [
                {
                  shaderLocation: 0,
                  offset: cubePositionOffset,
                  format: "float32x4",
                },
                {
                  shaderLocation: 1,
                  offset: cubeUVOffset,
                  format: "float32x2",
                },
              ],
            },
          ],
        },
        fragment: {
          module: device.createShaderModule({ code: vertexPositionColorWGSL }),
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list", cullMode: "back" },
        depthStencil: {
          depthWriteEnabled: true,
          depthCompare: "less",
          format: "depth24plus",
        },
      });

      let depthTexture = device.createTexture({
        size: [Math.max(canvas.width, 1), Math.max(canvas.height, 1)],
        format: "depth24plus",
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      });

      const uniformBuffer = device.createBuffer({
        size: 64,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      const uniformBindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
      });

      const modelViewProjectionMatrix = mat4.create();

      const getTransformationMatrix = (timestamp: number) => {
        const htmlCanvas = context.canvas as HTMLCanvasElement;
        const width = Math.max(htmlCanvas.width, 1);
        const height = Math.max(htmlCanvas.height, 1);
        const aspect = width / height;

        const projectionMatrix = mat4.perspective(
          (2 * Math.PI) / 5,
          aspect,
          1,
          100,
        );

        const viewMatrix = mat4.identity();
        mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
        const t = timestamp / 1000;
        mat4.rotate(
          viewMatrix,
          vec3.fromValues(Math.sin(t), Math.cos(t), 0),
          1,
          viewMatrix,
        );

        return mat4.multiply(
          projectionMatrix,
          viewMatrix,
          modelViewProjectionMatrix,
        );
      };

      return (timestamp: number) => {
        const htmlCanvas = context.canvas as HTMLCanvasElement;
        const dpr = PixelRatio.get();
        htmlCanvas.width = htmlCanvas.clientWidth * dpr;
        htmlCanvas.height = htmlCanvas.clientHeight * dpr;

        const width = Math.max(htmlCanvas.width, 1);
        const height = Math.max(htmlCanvas.height, 1);

        if (depthTexture.width !== width || depthTexture.height !== height) {
          depthTexture.destroy();
          depthTexture = device.createTexture({
            size: [width, height],
            format: "depth24plus",
            usage: GPUTextureUsage.RENDER_ATTACHMENT,
          });
        }

        const transformationMatrix = getTransformationMatrix(timestamp);
        device.queue.writeBuffer(
          uniformBuffer,
          0,
          transformationMatrix.buffer,
          transformationMatrix.byteOffset,
          transformationMatrix.byteLength,
        );

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: [0.05, 0.08, 0.14, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
          depthStencilAttachment: {
            view: depthTexture.createView(),
            depthClearValue: 1,
            depthLoadOp: "clear",
            depthStoreOp: "store",
          },
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, uniformBindGroup);
        pass.setVertexBuffer(0, verticesBuffer);
        pass.draw(cubeVertexCount);
        pass.end();
        device.queue.submit([encoder.finish()]);
      };
  }, []);

  const ref = useWebGPU(scene);

  return (
    <View style={styles.container}>
      <Canvas ref={ref} style={StyleSheet.absoluteFill} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1 },
});
