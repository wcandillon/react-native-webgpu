"use client";

import { useEffect, useRef } from "react";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import { Canvas, useDevice, type CanvasRef } from "react-native-webgpu";

import {
  transparencyFragWGSL,
  transparencyVertWGSL,
} from "./shaders/transparency";

function Background() {
  return (
    <View style={styles.background}>
      <View style={styles.row}>
        <View style={[styles.quadrant, { backgroundColor: "#3498db" }]} />
        <View style={[styles.quadrant, { backgroundColor: "#e67e22" }]} />
      </View>
      <View style={styles.row}>
        <View style={[styles.quadrant, { backgroundColor: "#9b59b6" }]} />
        <View style={[styles.quadrant, { backgroundColor: "#1abc9c" }]} />
      </View>
      <View style={styles.labelWrap} pointerEvents="none">
        <Text style={styles.label}>React Native views underneath</Text>
      </View>
    </View>
  );
}

export function TransparencyDemo() {
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

      context.configure({
        device,
        format,
        alphaMode: "premultiplied",
      });

      const uniformBuffer = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      const bindGroupLayout = device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.FRAGMENT,
            buffer: { type: "uniform" },
          },
        ],
      });

      const pipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] }),
        vertex: {
          module: device.createShaderModule({ code: transparencyVertWGSL }),
          entryPoint: "main",
        },
        fragment: {
          module: device.createShaderModule({ code: transparencyFragWGSL }),
          entryPoint: "main",
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
          new Float32Array([canvas.width, time * 0.001]),
        );

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 0],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(6);
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
      <Background />
      <Canvas ref={ref} style={StyleSheet.absoluteFill} transparent />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  background: {
    ...StyleSheet.absoluteFillObject,
  },
  row: {
    flex: 1,
    flexDirection: "row",
  },
  quadrant: {
    flex: 1,
  },
  labelWrap: {
    ...StyleSheet.absoluteFillObject,
    alignItems: "center",
    justifyContent: "center",
  },
  label: {
    color: "#ffffff",
    fontSize: 14,
    fontWeight: "600",
    letterSpacing: 0.2,
    opacity: 0.92,
    textAlign: "center",
    textShadowColor: "rgba(0, 0, 0, 0.45)",
    textShadowOffset: { width: 0, height: 1 },
    textShadowRadius: 4,
  },
});
