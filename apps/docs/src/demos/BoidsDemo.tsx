"use client";

import { useCallback } from "react";
import { StyleSheet, View } from "react-native";
import { Canvas } from "react-native-webgpu";

import {
  BOID_COUNT,
  boidsComputeWGSL,
  boidsRenderWGSL,
} from "./shaders/boids";
import { useWebGPU, type SceneProps } from "./useWebGPU";

function initBoids(): Float32Array {
  const data = new Float32Array(BOID_COUNT * 4);
  for (let i = 0; i < BOID_COUNT; i++) {
    data[i * 4] = Math.random() * 1.6 - 0.8;
    data[i * 4 + 1] = Math.random() * 1.6 - 0.8;
    data[i * 4 + 2] = (Math.random() - 0.5) * 0.01;
    data[i * 4 + 3] = (Math.random() - 0.5) * 0.01;
  }
  return data;
}

export function BoidsDemo() {
  const scene = useCallback(({ context, device, presentationFormat }: SceneProps) => {
    const bufferA = device.createBuffer({
      size: BOID_COUNT * 16,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });
    const bufferB = device.createBuffer({
      size: BOID_COUNT * 16,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });
    const initial = initBoids();
    device.queue.writeBuffer(
      bufferA,
      0,
      initial.buffer,
      initial.byteOffset,
      initial.byteLength,
    );

    const computeModule = device.createShaderModule({ code: boidsComputeWGSL });
    const computePipeline = device.createComputePipeline({
      layout: "auto",
      compute: { module: computeModule, entryPoint: "main" },
    });

    const renderModule = device.createShaderModule({ code: boidsRenderWGSL });
    const renderPipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: renderModule, entryPoint: "vs_main" },
      fragment: {
        module: renderModule,
        entryPoint: "fs_main",
        targets: [{ format: presentationFormat }],
      },
      primitive: { topology: "triangle-list" },
    });

    let readBuffer = bufferA;
    let writeBuffer = bufferB;

    return () => {
      const computeBindGroup = device.createBindGroup({
        layout: computePipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: { buffer: readBuffer } },
          { binding: 1, resource: { buffer: writeBuffer } },
        ],
      });

      const renderBindGroup = device.createBindGroup({
        layout: renderPipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: writeBuffer } }],
      });

      const encoder = device.createCommandEncoder();

      const computePass = encoder.beginComputePass();
      computePass.setPipeline(computePipeline);
      computePass.setBindGroup(0, computeBindGroup);
      computePass.dispatchWorkgroups(Math.ceil(BOID_COUNT / 64));
      computePass.end();

      const renderPass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: [0.05, 0.08, 0.14, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      renderPass.setPipeline(renderPipeline);
      renderPass.setBindGroup(0, renderBindGroup);
      renderPass.draw(3, BOID_COUNT);
      renderPass.end();

      device.queue.submit([encoder.finish()]);

      [readBuffer, writeBuffer] = [writeBuffer, readBuffer];
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
