import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import type { SharedValue } from "react-native-reanimated";
import { runOnUI, useSharedValue } from "react-native-reanimated";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

// Step 1 of the main-thread refactor: validate that we can render into a
// freestanding GPUTexture on the JS thread and have a UI-thread compositor
// blit it onto the canvas surface. Same shape we want to lift into C++ later.
const composeOnUI = (
  device: GPUDevice,
  context: RNCanvasContext,
  pool: GPUTexture[],
  latestReady: SharedValue<number>,
  running: SharedValue<boolean>,
  width: number,
  height: number,
  presentationFormat: GPUTextureFormat,
  surfaceUsage: number,
) => {
  "worklet";
  context.configure({
    device,
    format: presentationFormat,
    usage: surfaceUsage,
    alphaMode: "premultiplied",
  });
  const compose = () => {
    const i = latestReady.value;
    if (i >= 0) {
      const source = pool[i];
      const encoder = device.createCommandEncoder();
      encoder.copyTextureToTexture(
        { texture: source },
        { texture: context.getCurrentTexture() },
        [width, height, 1],
      );
      device.queue.submit([encoder.finish()]);
      context.present();
    }
    if (running.value) {
      requestAnimationFrame(compose);
    }
  };
  compose();
};

export function ReanimatedOffscreen() {
  const running = useSharedValue(true);
  const latestReady = useSharedValue(-1);
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    let producerRaf: number | null = null;
    let cancelled = false;

    const init = async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        console.error("[ReanimatedOffscreen] no GPU adapter");
        return;
      }
      const device = await adapter.requestDevice();
      if (!device) {
        console.error("[ReanimatedOffscreen] no GPU device");
        return;
      }
      if (cancelled || !ref.current) {
        return;
      }
      const ctx = ref.current.getContext("webgpu");
      if (!ctx) {
        console.error("[ReanimatedOffscreen] no canvas context");
        return;
      }
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      const native = ref.current.getNativeSurface();
      const width = Math.max(1, Math.floor(native.width));
      const height = Math.max(1, Math.floor(native.height));

      const poolUsage =
        GPUTextureUsage.RENDER_ATTACHMENT |
        GPUTextureUsage.COPY_SRC |
        GPUTextureUsage.TEXTURE_BINDING;
      const pool: GPUTexture[] = [
        device.createTexture({
          size: [width, height],
          format: presentationFormat,
          usage: poolUsage,
        }),
        device.createTexture({
          size: [width, height],
          format: presentationFormat,
          usage: poolUsage,
        }),
      ];

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

      // Surface must accept the blit destination role.
      const surfaceUsage =
        GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_DST;

      runOnUI(composeOnUI)(
        device,
        ctx,
        pool,
        latestReady,
        running,
        width,
        height,
        presentationFormat,
        surfaceUsage,
      );

      let writeIndex = 0;
      const produce = () => {
        if (cancelled || !running.value) {
          return;
        }
        const i = writeIndex;
        const target = pool[i];
        const time = Date.now() / 1000;
        const r = (Math.sin(time * 2) + 1) / 2;
        const g = (Math.sin(time * 1.5 + Math.PI / 3) + 1) / 2;
        const b = (Math.sin(time * 1 + Math.PI / 2) + 1) / 2;

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: target.createView(),
              clearValue: [r, g, b, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(pipeline);
        pass.draw(3);
        pass.end();
        device.queue.submit([encoder.finish()]);

        // Publish under single-queue ordering: submits from this runtime are
        // serialized with the compositor's submits, so reading this index on
        // the UI runtime sees the texture in its post-submit state.
        latestReady.value = i;
        writeIndex = 1 - i;
        producerRaf = requestAnimationFrame(produce);
      };
      producerRaf = requestAnimationFrame(produce);
    };
    init();

    return () => {
      cancelled = true;
      running.value = false;
      if (producerRaf !== null) {
        cancelAnimationFrame(producerRaf);
      }
    };
  }, [running, latestReady]);

  return (
    <View style={styles.container}>
      <Canvas ref={ref} style={styles.webgpu} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "rgb(90, 180, 255)",
  },
  webgpu: {
    flex: 1,
  },
});
