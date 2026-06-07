import React, { useEffect, useRef, useState } from "react";
import { Pressable, StyleSheet, Text, View } from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import { Canvas, GPUBufferUsage, GPUMapMode } from "react-native-webgpu";
import type { SharedValue } from "react-native-reanimated";
import { useSharedValue } from "react-native-reanimated";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

// A triangle demo that creates its adapter/device AND performs an async GPU
// readback (buffer.mapAsync) every frame, all on the runtime this worklet runs
// on. With the ProcessEvents async model the device must be created and used on
// the same runtime, so requestAdapter/requestDevice happen here in the worklet
// (the GPU object is passed in). The point: with the JS thread busy, the readback
// keeps resolving on this runtime's own thread and the triangle keeps animating.
//
// GPUBufferUsage / GPUMapMode are imported from react-native-webgpu: the bare
// globals are only installed on the main JS runtime, but importing them lets the
// Worklets serializer capture them by closure, so they work on this runtime too.
export const webGPUAsyncDemo = (
  runAnimation: SharedValue<boolean>,
  context: RNCanvasContext,
  gpu: GPU,
  presentationFormat: GPUTextureFormat,
) => {
  "worklet";
  if (!context) {
    throw new Error("No context");
  }

  // Errors thrown on a worklet are forwarded to the JS thread by the worklets
  // runtime; if the error object transitively references WebGPU host objects,
  // JSON.stringify of it on the JS side can crash. So we catch everything here
  // and forward only a plain string.
  const logError = (where: string, e: unknown) => {
    console.error(
      `[asyncBuffer] ${where}: ` +
        String((e as { message?: string })?.message ?? e),
    );
  };

  const run = async () => {
    const adapter = await gpu.requestAdapter();
    if (!adapter) {
      console.error("[asyncBuffer] failed to get adapter on worklet runtime");
      return;
    }
    const device = await adapter.requestDevice();
    if (!device) {
      console.error("[asyncBuffer] failed to get device on worklet runtime");
      return;
    }
    console.log("[asyncBuffer] device created on worklet runtime");

    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
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

    const SIZE = 16; // 4 x f32
    const readback = device.createBuffer({
      size: SIZE,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });

    let frameId = 0;

    const frame = async () => {
      try {
        frameId += 1;
        const commandEncoder = device.createCommandEncoder();
        const textureView = context.getCurrentTexture().createView();

        const time = Date.now() / 1000;
        const r = (Math.sin(time * 2) + 1) / 2;
        const g = (Math.sin(time * 1.5 + Math.PI / 3) + 1) / 2;
        const b = (Math.sin(time + Math.PI / 2) + 1) / 2;

        const passEncoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              view: textureView,
              clearValue: [r, g, b, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        passEncoder.setPipeline(pipeline);
        passEncoder.draw(3);
        passEncoder.end();

        const src = device.createBuffer({
          size: SIZE,
          usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.MAP_WRITE,
          mappedAtCreation: true,
        });
        new Float32Array(src.getMappedRange()).set([frameId, r, g, b]);
        src.unmap();
        commandEncoder.copyBufferToBuffer(src, 0, readback, 0, SIZE);

        device.queue.submit([commandEncoder.finish()]);

        // THE ASYNC OP. With the ProcessEvents model this Promise is pumped and
        // settled on THIS runtime's own thread, so it resolves even while the JS
        // thread is busy. Watch the logs against the "Make JS busy" button.
        await readback.mapAsync(GPUMapMode.READ);
        const data = Array.from(new Float32Array(readback.getMappedRange()));
        readback.unmap();
        src.destroy();
        if (frameId % 30 === 0) {
          console.log(`[asyncBuffer] frame ${frameId} resolved ->`, data);
        }

        context.present();

        if (runAnimation.value) {
          requestAnimationFrame(frame);
        }
      } catch (e) {
        logError("frame", e);
      }
    };
    frame();
  };
  run().catch((e) => logError("run", e));
};

interface AsyncBufferExampleProps {
  // Schedules the worklet on a given runtime (e.g. runOnUI for the UI thread,
  // or runOnRuntime(runtime, ...) for a dedicated worklet runtime).
  run: (
    worklet: typeof webGPUAsyncDemo,
  ) => (
    runAnimation: SharedValue<boolean>,
    context: RNCanvasContext,
    gpu: GPU,
    presentationFormat: GPUTextureFormat,
  ) => void;
}

export function AsyncBufferExample({ run }: AsyncBufferExampleProps) {
  const runAnimation = useSharedValue(true);
  const ref = useRef<CanvasRef>(null);
  const [busy, setBusy] = useState(false);

  // Hammer the JS thread to prove the worklet's async readback + rendering are
  // independent of it. Each tick blocks the JS thread for 250ms.
  useEffect(() => {
    if (!busy) {
      return;
    }
    let job = requestAnimationFrame(function work() {
      const start = Date.now();
      while (Date.now() - start < 250) {
        // Busy-wait, blocking the JS thread.
      }
      job = requestAnimationFrame(work);
    });
    return () => cancelAnimationFrame(job);
  }, [busy]);

  useEffect(() => {
    const ctx = ref.current!.getContext("webgpu");
    if (!ctx) {
      console.error("Failed to get GPU canvas context");
      return;
    }
    // The GPU object is created on the main runtime; we hand it to the worklet,
    // which calls requestAdapter/requestDevice on its OWN runtime.
    const { gpu } = navigator;
    const presentationFormat = gpu.getPreferredCanvasFormat();
    run(webGPUAsyncDemo)(runAnimation, ctx, gpu, presentationFormat);
    return () => {
      runAnimation.value = false;
    };
    // Init the GPU pipeline once on mount. Toggling `busy` must NOT re-run this
    // (a second device + render loop would fight over the same surface and
    // trigger a device-mismatch validation error).
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
      <Pressable style={style.button} onPress={() => setBusy((b) => !b)}>
        <Text style={style.buttonText}>
          {busy ? "Stop busy JS" : "Make JS busy"}
        </Text>
      </Pressable>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "rgb(90, 180, 255)",
  },
  webgpu: {
    flex: 1,
  },
  button: {
    position: "absolute",
    bottom: 32,
    alignSelf: "center",
    backgroundColor: "rgba(0,0,0,0.6)",
    paddingHorizontal: 20,
    paddingVertical: 12,
    borderRadius: 24,
  },
  buttonText: {
    color: "white",
    fontSize: 16,
    fontWeight: "600",
  },
});
