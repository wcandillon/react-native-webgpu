import React, { useEffect, useRef } from "react";
import { StyleSheet, Text, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import type { CanvasRef } from "react-native-wgpu";
import { mat4, vec3 } from "wgpu-matrix";

import { cubeVertexCount } from "../components/cube";

const NUM_ITEMS = 50;

export interface SharedResources {
  pipeline: GPURenderPipeline;
  verticesBuffer: GPUBuffer;
  presentationFormat: GPUTextureFormat;
}

interface CubeItemProps {
  index: number;
  itemHeight: number;
  sharedResources: SharedResources;
  device: GPUDevice;
}

function hslToRgb(h: number, s: number, l: number): [number, number, number] {
  const c = (1 - Math.abs(2 * l - 1)) * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = l - c / 2;
  let r = 0,
    g = 0,
    b = 0;
  if (h < 60) {
    r = c;
    g = x;
    b = 0;
  } else if (h < 120) {
    r = x;
    g = c;
    b = 0;
  } else if (h < 180) {
    r = 0;
    g = c;
    b = x;
  } else if (h < 240) {
    r = 0;
    g = x;
    b = c;
  } else if (h < 300) {
    r = x;
    g = 0;
    b = c;
  } else {
    r = c;
    g = 0;
    b = x;
  }
  return [r + m, g + m, b + m];
}

function indexToColor(index: number): [number, number, number] {
  const hue = (index / NUM_ITEMS) * 360;
  return hslToRgb(hue, 0.8, 0.6);
}

export function CubeItem({
  index,
  itemHeight,
  sharedResources,
  device,
}: CubeItemProps) {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }

    const { pipeline, verticesBuffer, presentationFormat } = sharedResources;
    const canvas = context.canvas as HTMLCanvasElement;

    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    // Per-item uniform buffer: mat4x4 (64 bytes) + vec4 tint (16 bytes) = 80 bytes
    const uniformBufferSize = 4 * 16 + 4 * 4;
    const uniformBuffer = device.createBuffer({
      size: uniformBufferSize,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const depthTexture = device.createTexture({
      size: [canvas.width, canvas.height],
      format: "depth24plus",
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
    });

    // Static MVP matrix with fixed rotation based on index
    const aspect = canvas.width / canvas.height;
    const projectionMatrix = mat4.perspective(
      (2 * Math.PI) / 5,
      aspect,
      1,
      100.0,
    );
    const viewMatrix = mat4.identity();
    mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
    const angle = index * 0.4 + 0.5;
    mat4.rotate(
      viewMatrix,
      vec3.fromValues(Math.sin(angle), Math.cos(angle), 0),
      1,
      viewMatrix,
    );
    const mvp = mat4.multiply(projectionMatrix, viewMatrix);

    // Write MVP matrix
    device.queue.writeBuffer(
      uniformBuffer,
      0,
      mvp.buffer,
      mvp.byteOffset,
      mvp.byteLength,
    );

    // Write tint color
    const [r, g, b] = indexToColor(index);
    device.queue.writeBuffer(
      uniformBuffer,
      4 * 16,
      new Float32Array([r, g, b, 1.0]),
    );

    // Render one frame
    const renderPassDescriptor: GPURenderPassDescriptor = {
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [0.15, 0.15, 0.15, 1],
          loadOp: "clear" as const,
          storeOp: "store" as const,
        },
      ],
      depthStencilAttachment: {
        view: depthTexture.createView(),
        depthClearValue: 1.0,
        depthLoadOp: "clear" as const,
        depthStoreOp: "store" as const,
      },
    };

    const commandEncoder = device.createCommandEncoder();
    const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
    passEncoder.setPipeline(pipeline);
    passEncoder.setBindGroup(0, bindGroup);
    passEncoder.setVertexBuffer(0, verticesBuffer);
    passEncoder.draw(cubeVertexCount);
    passEncoder.end();
    device.queue.submit([commandEncoder.finish()]);
    context.present();

    return () => {
      uniformBuffer.destroy();
      depthTexture.destroy();
    };
  }, [ref, device, index, sharedResources]);

  return (
    <View style={[styles.item, { height: itemHeight }]}>
      <Text style={styles.label}>Context #{index}</Text>
      <Canvas ref={ref} style={styles.canvas} />
    </View>
  );
}

const styles = StyleSheet.create({
  item: {
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: "#333",
    backgroundColor: "#1a1a1a",
  },
  label: {
    color: "#fff",
    fontSize: 12,
    paddingHorizontal: 12,
    paddingTop: 8,
    paddingBottom: 4,
  },
  canvas: {
    flex: 1,
  },
});
