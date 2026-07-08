"use client";

import { useEffect, useRef } from "react";
import { PixelRatio, StyleSheet, View } from "react-native";
import { Canvas, type CanvasRef } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "./shaders/triangle";

interface HelloTriangleDemoProps {
  transparent?: boolean;
}

export function HelloTriangleDemo({ transparent = true }: HelloTriangleDemoProps) {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    let cancelled = false;

    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter || cancelled) {
        return;
      }

      const device = await adapter.requestDevice();
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      const context = ref.current?.getContext("webgpu");
      if (!context || cancelled) {
        return;
      }

      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: transparent ? "premultiplied" : "opaque",
      });

      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: device.createShaderModule({ code: triangleVertWGSL }),
          entryPoint: "main",
        },
        fragment: {
          module: device.createShaderModule({ code: redFragWGSL }),
          entryPoint: "main",
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list" },
      });

      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: transparent ? [0, 0, 0, 0] : [0.05, 0.08, 0.14, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(pipeline);
      pass.draw(3);
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
    })();

    return () => {
      cancelled = true;
    };
  }, [transparent]);

  return (
    <View style={styles.container}>
      {transparent ? (
        <View style={{ flex: 1, backgroundColor: "#3498db" }} />
      ) : null}
      <Canvas
        ref={ref}
        style={StyleSheet.absoluteFill}
        transparent={transparent}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
});
