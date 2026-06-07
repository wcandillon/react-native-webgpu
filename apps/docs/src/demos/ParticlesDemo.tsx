"use client";

import { useCallback } from "react";
import { StyleSheet, View } from "react-native";
import { Canvas } from "react-native-webgpu";

import {
  PARTICLE_COUNT,
  particlesComputeWGSL,
  particlesRenderWGSL,
} from "./shaders/particles";
import { useWebGPU, type SceneProps } from "./useWebGPU";

function initParticles(): Float32Array {
  const data = new Float32Array(PARTICLE_COUNT * 4);
  for (let i = 0; i < PARTICLE_COUNT; i++) {
    data[i * 4] = Math.random() * 1.8 - 0.9;
    data[i * 4 + 1] = Math.random() * 1.8 - 0.9;
    data[i * 4 + 2] = (Math.random() - 0.5) * 0.008;
    data[i * 4 + 3] = 0.004 + Math.random() * 0.008;
  }
  return data;
}

export function ParticlesDemo() {
  const scene = useCallback(({ context, device, presentationFormat }: SceneProps) => {
    const particlesBuffer = device.createBuffer({
      size: PARTICLE_COUNT * 16,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });
    const initial = initParticles();
    device.queue.writeBuffer(
      particlesBuffer,
      0,
      initial.buffer,
      initial.byteOffset,
      initial.byteLength,
    );

    const paramsBuffer = device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const computePipeline = device.createComputePipeline({
      layout: "auto",
      compute: {
        module: device.createShaderModule({ code: particlesComputeWGSL }),
        entryPoint: "main",
      },
    });

    const renderModule = device.createShaderModule({ code: particlesRenderWGSL });
    const renderPipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: renderModule, entryPoint: "vs_main" },
      fragment: {
        module: renderModule,
        entryPoint: "fs_main",
        targets: [{
          format: presentationFormat,
          blend: {
            color: { srcFactor: "src-alpha", dstFactor: "one", operation: "add" },
            alpha: { srcFactor: "one", dstFactor: "one", operation: "add" },
          },
        }],
      },
      primitive: { topology: "triangle-list" },
    });

    return () => {
      device.queue.writeBuffer(paramsBuffer, 0, new Float32Array([1.0, 0.00005]));

      const encoder = device.createCommandEncoder();

      const computePass = encoder.beginComputePass();
      computePass.setPipeline(computePipeline);
      computePass.setBindGroup(0, device.createBindGroup({
        layout: computePipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: { buffer: particlesBuffer } },
          { binding: 1, resource: { buffer: paramsBuffer } },
        ],
      }));
      computePass.dispatchWorkgroups(Math.ceil(PARTICLE_COUNT / 256));
      computePass.end();

      const renderPass = encoder.beginRenderPass({
        colorAttachments: [{
          view: context.getCurrentTexture().createView(),
          clearValue: [0.02, 0.02, 0.05, 1],
          loadOp: "clear",
          storeOp: "store",
        }],
      });
      renderPass.setPipeline(renderPipeline);
      renderPass.setBindGroup(0, device.createBindGroup({
        layout: renderPipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: particlesBuffer } }],
      }));
      renderPass.draw(6, PARTICLE_COUNT);
      renderPass.end();

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
