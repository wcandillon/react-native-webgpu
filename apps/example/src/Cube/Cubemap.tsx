import React from "react";
import { Dimensions, StyleSheet, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import { mat4, vec3 } from "wgpu-matrix";

import { useWebGPU } from "../components/useWebGPU";
import {
  cubePositionOffset,
  cubeUVOffset,
  cubeVertexArray,
  cubeVertexCount,
  cubeVertexSize,
} from "../components/cube";
import { fetchAsset } from "../components/useAssets";

import { basicVertWGSL, sampleCubemapWGSL } from "./Shaders";

const size = Dimensions.get("window").width;

export const Cubemap = () => {
  const ref = useWebGPU(
    async ({ context, device, presentationFormat, canvas }) => {
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
            code: sampleCubemapWGSL,
          }),
          targets: [
            {
              format: presentationFormat,
            },
          ],
        },
        primitive: {
          topology: "triangle-list",

          // Since we are seeing from inside of the cube
          // and we are using the regular cube geomtry data with outward-facing normals,
          // the cullMode should be 'front' or 'none'.
          cullMode: "none",
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

      // Fetch the 6 separate images for negative/positive x, y, z axis of a cubemap
      // and upload it into a GPUTexture.
      let cubemapTexture: GPUTexture;
      {
        // The order of the array layers is [+X, -X, +Y, -Y, +Z, -Z]
        const imgSrcs = [
          require("../assets/cubemap/posx.jpg"),
          require("../assets/cubemap/negx.jpg"),
          require("../assets/cubemap/posy.jpg"),
          require("../assets/cubemap/negy.jpg"),
          require("../assets/cubemap/posz.jpg"),
          require("../assets/cubemap/negz.jpg"),
        ];
        const promises = imgSrcs.map(async (src) => {
          const response = await fetchAsset(src);
          return createImageBitmap(await response.blob());
        });
        const imageBitmaps = await Promise.all(promises);

        cubemapTexture = device.createTexture({
          dimension: "2d",
          // Create a 2d array texture.
          // Assume each image has the same size.
          size: [imageBitmaps[0].width, imageBitmaps[0].height, 6],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.TEXTURE_BINDING |
            GPUTextureUsage.COPY_DST |
            GPUTextureUsage.RENDER_ATTACHMENT,
        });

        for (let i = 0; i < imageBitmaps.length; i++) {
          const imageBitmap = imageBitmaps[i];
          device.queue.copyExternalImageToTexture(
            { source: imageBitmap },
            { texture: cubemapTexture, origin: [0, 0, i] },
            [imageBitmap.width, imageBitmap.height],
          );
        }
      }

      const uniformBufferSize = 4 * 16; // 4x4 matrix
      const uniformBuffer = device.createBuffer({
        size: uniformBufferSize,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      const sampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear",
      });

      const uniformBindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: uniformBuffer,
              offset: 0,
              size: uniformBufferSize,
            },
          },
          {
            binding: 1,
            resource: sampler,
          },
          {
            binding: 2,
            resource: cubemapTexture.createView({
              dimension: "cube",
            }),
          },
        ],
      });

      const renderPassDescriptor: GPURenderPassDescriptor = {
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-expect-error
        colorAttachments: [
          {
            view: undefined, // Assigned later
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
        3000,
      );

      const modelMatrix = mat4.scaling(vec3.fromValues(1000, 1000, 1000));
      const modelViewProjectionMatrix = mat4.create();
      const viewMatrix = mat4.identity();

      const tmpMat4 = mat4.create();

      // Comppute camera movement:
      // It rotates around Y axis with a slight pitch movement.
      function updateTransformationMatrix() {
        const now = Date.now() / 800;

        mat4.rotate(
          viewMatrix,
          vec3.fromValues(1, 0, 0),
          (Math.PI / 10) * Math.sin(now),
          tmpMat4,
        );
        mat4.rotate(tmpMat4, vec3.fromValues(0, 1, 0), now * 0.2, tmpMat4);

        mat4.multiply(tmpMat4, modelMatrix, modelViewProjectionMatrix);
        mat4.multiply(
          projectionMatrix,
          modelViewProjectionMatrix,
          modelViewProjectionMatrix,
        );
      }

      function frame() {
        updateTransformationMatrix();
        device.queue.writeBuffer(
          uniformBuffer,
          0,
          modelViewProjectionMatrix.buffer,
          modelViewProjectionMatrix.byteOffset,
          modelViewProjectionMatrix.byteLength,
        );

        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-expect-error
        renderPassDescriptor.colorAttachments[0].view = context
          .getCurrentTexture()
          .createView();

        const commandEncoder = device.createCommandEncoder();
        const passEncoder =
          commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(pipeline);
        passEncoder.setVertexBuffer(0, verticesBuffer);
        passEncoder.setBindGroup(0, uniformBindGroup);
        passEncoder.draw(cubeVertexCount);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
      }
      return frame;
    },
  );

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
    </View>
  );
};

const style = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: "center",
    backgroundColor: "rgb(128, 128, 128)",
  },
  webgpu: {
    width: size,
    height: size,
  },
});
