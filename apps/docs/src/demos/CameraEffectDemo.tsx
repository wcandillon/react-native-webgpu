"use client";

import { useEffect, useRef } from "react";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import { Canvas, useDevice, type CanvasRef } from "react-native-webgpu";

import {
  cameraEffectFragWGSL,
  cameraEffectVertWGSL,
} from "./shaders/cameraEffect";

export function CameraEffectDemo() {
  const { device } = useDevice();
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    if (!device) {
      return;
    }

    let animationId = 0;
    let cancelled = false;

    const start = () => {
      const context = ref.current?.getContext("webgpu");
      if (!context || cancelled) {
        animationId = requestAnimationFrame(start);
        return;
      }

      const canvas = context.canvas as HTMLCanvasElement;
      if (canvas.clientWidth === 0 || canvas.clientHeight === 0) {
        animationId = requestAnimationFrame(start);
        return;
      }

      const format = navigator.gpu.getPreferredCanvasFormat();
      const dpr = PixelRatio.get();
      canvas.width = canvas.clientWidth * dpr;
      canvas.height = canvas.clientHeight * dpr;

      context.configure({ device, format, alphaMode: "opaque" });

      const uniformBuffer = device.createBuffer({
        size: 32,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      const bindGroupLayout = device.createBindGroupLayout({
        entries: [{
          binding: 0,
          visibility: GPUShaderStage.FRAGMENT,
          buffer: { type: "uniform" },
        }],
      });

      const pipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] }),
        vertex: {
          module: device.createShaderModule({ code: cameraEffectVertWGSL }),
          entryPoint: "vs_main",
        },
        fragment: {
          module: device.createShaderModule({ code: cameraEffectFragWGSL }),
          entryPoint: "fs_main",
          targets: [{ format }],
        },
        primitive: { topology: "triangle-list" },
      });

      const bindGroup = device.createBindGroup({
        layout: bindGroupLayout,
        entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
      });

      const render = (time: number) => {
        if (cancelled) {
          return;
        }

        canvas.width = canvas.clientWidth * dpr;
        canvas.height = canvas.clientHeight * dpr;

        device.queue.writeBuffer(
          uniformBuffer,
          0,
          new Float32Array([
            time * 0.001,
            0.006,
            0.7,
            canvas.width,
            canvas.height,
          ]),
        );

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [{
            view: context.getCurrentTexture().createView(),
            clearValue: [0.05, 0.05, 0.08, 1],
            loadOp: "clear",
            storeOp: "store",
          }],
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
        pass.end();
        device.queue.submit([encoder.finish()]);
        context.present();

        animationId = requestAnimationFrame(render);
      };

      animationId = requestAnimationFrame(render);
    };

    animationId = requestAnimationFrame(start);

    return () => {
      cancelled = true;
      cancelAnimationFrame(animationId);
    };
  }, [device]);

  return (
    <View style={styles.container}>
      <Canvas ref={ref} style={StyleSheet.absoluteFill} />
      <View style={styles.badge} pointerEvents="none">
        <Text style={styles.badgeText}>Simulated camera feed</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1 },
  badge: {
    position: "absolute",
    bottom: 8,
    left: 8,
    backgroundColor: "rgba(0,0,0,0.55)",
    borderRadius: 6,
    paddingHorizontal: 8,
    paddingVertical: 4,
  },
  badgeText: {
    color: "#fff",
    fontSize: 11,
    fontWeight: "500",
  },
});
