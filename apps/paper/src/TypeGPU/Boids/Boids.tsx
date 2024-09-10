import React from "react";
import tgpu from "typegpu";
import { arrayOf, f32, vec2f, struct } from "typegpu/data";
import { StyleSheet, View } from "react-native";
import { Canvas } from "react-native-wgpu";

import { useWebGPU } from "../../components/useWebGPU";
import { toBeAssignedLater } from "../../components/utils";

import { renderCode, computeCode } from "./Shaders";

export function Boids() {
  const { canvasRef } = useWebGPU(({ context, device, presentationFormat }) => {
    console.log("Running Boids example.");
    console.log("Configuring context.");
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    console.log("Creating buffers.");
    const params = struct({
      separationDistance: f32,
      separationStrength: f32,
      alignmentDistance: f32,
      alignmentStrength: f32,
      cohesionDistance: f32,
      cohesionStrength: f32,
    });

    console.log("Creating params buffer.");
    const paramsBuffer = tgpu
      .createBuffer(params, {
        separationDistance: 0.05,
        separationStrength: 0.001,
        alignmentDistance: 0.3,
        alignmentStrength: 0.01,
        cohesionDistance: 0.3,
        cohesionStrength: 0.001,
      })
      .$device(device)
      .$usage(tgpu.Storage);

    console.log("Creating triangle vertex buffer.");
    const triangleSize = 0.02;
    const triangleVertexBuffer = tgpu
      .createBuffer(arrayOf(f32, 6), [
        0.0,
        triangleSize,
        -triangleSize / 2,
        -triangleSize / 2,
        triangleSize / 2,
        -triangleSize / 2,
      ])
      .$device(device)
      .$usage(tgpu.Vertex);

    const triangleAmount = 1000;
    console.log("Creating triangle position buffers 1.");
    const triangleInfoStruct = struct({
      positionX: f32,
      positionY: f32,
      velocityX: f32,
      velocityY: f32,
    });
    console.log("Creating triangle position buffers 2.");
    const trianglePosBuffers = Array.from({ length: 2 }, () =>
      tgpu
        .createBuffer(arrayOf(triangleInfoStruct, triangleAmount))
        .$device(device)
        .$usage(tgpu.Storage, tgpu.Uniform),
    );

    console.log("Randomizing positions.");
    const randomizePositions = () => {
      const positions = Array.from({ length: triangleAmount }, () => ({
        positionX: Math.random() * 2 - 1,
        positionY: Math.random() * 2 - 1,
        velocityX: Math.random() * 0.1 - 0.05,
        velocityY: Math.random() * 0.1 - 0.05,
      }));
      tgpu.write(trianglePosBuffers[0], positions);
      tgpu.write(trianglePosBuffers[1], positions);
    };
    randomizePositions();

    console.log("Creating shader modules.");
    const renderModule = device.createShaderModule({
      code: renderCode,
    });

    const computeModule = device.createShaderModule({
      code: computeCode,
    });

    console.log("Creating render pipeline.");
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: {
        module: renderModule,
        buffers: [
          {
            arrayStride: 2 * 4,
            attributes: [
              {
                shaderLocation: 0,
                offset: 0,
                format: "float32x2",
              },
            ],
          },
        ],
      },
      fragment: {
        module: renderModule,
        targets: [
          {
            format: presentationFormat,
          },
        ],
      },
      primitive: {
        topology: "triangle-list",
      },
    });

    console.log("Creating compute pipeline.");
    const computePipeline = device.createComputePipeline({
      layout: "auto",
      compute: {
        module: computeModule,
      },
    });

    console.log("Creating bind groups.");
    const renderBindGroups = [0, 1].map((idx) =>
      device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: trianglePosBuffers[idx].buffer,
            },
          },
        ],
      }),
    );

    const computeBindGroups = [0, 1].map((idx) =>
      device.createBindGroup({
        layout: computePipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: {
              buffer: trianglePosBuffers[idx].buffer,
            },
          },
          {
            binding: 1,
            resource: {
              buffer: trianglePosBuffers[1 - idx].buffer,
            },
          },
          {
            binding: 2,
            resource: {
              buffer: paramsBuffer.buffer,
            },
          },
        ],
      }),
    );

    console.log("Setting up render pass descriptor.");
    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        {
          view: toBeAssignedLater(),
          clearValue: [1, 1, 1, 1],
          loadOp: "clear" as const,
          storeOp: "store" as const,
        },
      ],
    };

    let even = false;
    function frame() {
      console.log("Even?", even);
      even = !even;
      console.log("Getting current texture view.");
      // eslint-disable-next-line @typescript-eslint/ban-ts-comment
      // @ts-expect-error
      renderPassDescriptor.colorAttachments[0].view = context
        .getCurrentTexture()
        .createView();

      console.log("Creating command encoder.");
      const commandEncoder = device.createCommandEncoder();
      console.log("Beginning compute pass.");
      const computePass = commandEncoder.beginComputePass();
      computePass.setPipeline(computePipeline);
      computePass.setBindGroup(
        0,
        even ? computeBindGroups[0] : computeBindGroups[1],
      );
      computePass.dispatchWorkgroups(triangleAmount);
      computePass.end();

      console.log("Beginning render pass.");
      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.setVertexBuffer(0, triangleVertexBuffer.buffer);
      passEncoder.setBindGroup(
        0,
        even ? renderBindGroups[1] : renderBindGroups[0],
      );
      passEncoder.draw(3, triangleAmount);
      passEncoder.end();

      console.log("Submitting command encoder.");
      device.queue.submit([commandEncoder.finish()]);
    }
    return frame;
  });

  return (
    <View style={style.container}>
      <Canvas ref={canvasRef} style={style.webgpu} />
      <View style={style.controls}>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>span x: </Text>
          <Button
            title="➖"
            onPress={() => {
              setSpanX((prevSpan) => (prevSpan > 1 ? prevSpan - 1 : prevSpan));
            }}
          />
          <Button
            title="➕"
            onPress={() => {
              setSpanX((prevSpan) => (prevSpan < 10 ? prevSpan + 1 : prevSpan));
            }}
          />
        </View>

        <View style={style.buttonRow}>
          <Text style={style.spanText}>span y: </Text>
          <Button
            title="➖"
            onPress={() => {
              setSpanY((prevSpan) => (prevSpan > 1 ? prevSpan - 1 : prevSpan));
            }}
          />
          <Button
            title="➕"
            onPress={() => {
              setSpanY((prevSpan) => (prevSpan < 10 ? prevSpan + 1 : prevSpan));
            }}
          />
        </View>
      </View>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    aspectRatio: 1,
  },
  spanText: {
    fontSize: 20,
    fontWeight: "bold",
  },
  controls: {
    flex: 1,
    justifyContent: "center",
  },
  buttonRow: {
    flexDirection: "row",
    justifyContent: "center",
    alignItems: "center",
  },
});
