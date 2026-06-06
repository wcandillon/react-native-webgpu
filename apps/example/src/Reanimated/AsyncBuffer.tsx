import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";
import type { SharedValue } from "react-native-reanimated";
import { useSharedValue } from "react-native-reanimated";

import { redFragWGSL, triangleVertWGSL } from "../Triangle/triangle";

// The GPU usage / map-mode constants are plain numbers. We resolve them on the
// JS thread (where the constants are guaranteed to be installed) and pass them
// into the worklet, so the worklet does not depend on those globals being
// present on the UI / dedicated runtime.
interface GPUFlags {
  COPY_SRC: number;
  COPY_DST: number;
  MAP_READ: number;
  MAP_WRITE: number;
  MAP_MODE_READ: number;
}

// A triangle demo that ALSO performs an async GPU readback (buffer.mapAsync)
// every frame, then presents only after the readback resolves. This makes the
// behaviour of async WebGPU ops on the runtime visible: if the mapAsync Promise
// never settles on this runtime, the animation freezes after the first frame.
export const webGPUAsyncDemo = (
  runAnimation: SharedValue<boolean>,
  device: GPUDevice,
  context: RNCanvasContext,
  presentationFormat: GPUTextureFormat,
  flags: GPUFlags,
) => {
  "worklet";
  if (!context) {
    throw new Error("No context");
  }

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
  // Reused across frames: we copy 4 floats into this buffer and read them back.
  const readback = device.createBuffer({
    size: SIZE,
    usage: flags.COPY_DST | flags.MAP_READ,
  });

  let frameId = 0;

  const frame = async () => {
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

    // Put real data on the GPU so the readback below has something to wait on.
    const src = device.createBuffer({
      size: SIZE,
      usage: flags.COPY_SRC | flags.MAP_WRITE,
      mappedAtCreation: true,
    });
    new Float32Array(src.getMappedRange()).set([frameId, r, g, b]);
    src.unmap();
    commandEncoder.copyBufferToBuffer(src, 0, readback, 0, SIZE);

    device.queue.submit([commandEncoder.finish()]);

    // THE ASYNC OP. This Promise must settle on the runtime that is running
    // this worklet. On the JS thread it does. On the UI / dedicated runtime the
    // settlement currently routes through the main JS CallInvoker, so this
    // await may resolve on the wrong runtime (or never here), freezing the loop.
    console.log(`[asyncBuffer] frame ${frameId}: awaiting mapAsync...`);
    await readback.mapAsync(flags.MAP_MODE_READ);
    const data = Array.from(new Float32Array(readback.getMappedRange()));
    readback.unmap();
    src.destroy();
    console.log(`[asyncBuffer] frame ${frameId}: resolved ->`, data);

    // Present only AFTER the async readback resolves, so a stuck await visibly
    // freezes the animation instead of silently dropping the readback.
    context.present();

    if (runAnimation.value) {
      requestAnimationFrame(frame);
    }
  };
  frame();
};

interface AsyncBufferExampleProps {
  // Schedules the worklet on a given runtime (e.g. runOnUI for the UI thread,
  // or runOnRuntime(runtime, ...) for a dedicated worklet runtime).
  run: (
    worklet: typeof webGPUAsyncDemo,
  ) => (
    runAnimation: SharedValue<boolean>,
    device: GPUDevice,
    context: RNCanvasContext,
    presentationFormat: GPUTextureFormat,
    flags: GPUFlags,
  ) => void;
}

export function AsyncBufferExample({ run }: AsyncBufferExampleProps) {
  const runAnimation = useSharedValue(true);
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    const initWebGPU = async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        console.error("Failed to get GPU adapter");
        return;
      }
      const device = await adapter.requestDevice();
      if (!device) {
        console.error("Failed to get GPU device");
        return;
      }
      const ctx = ref.current!.getContext("webgpu");
      if (!ctx) {
        console.error("Failed to get GPU canvas context");
        return;
      }
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
      const flags: GPUFlags = {
        COPY_SRC: GPUBufferUsage.COPY_SRC,
        COPY_DST: GPUBufferUsage.COPY_DST,
        MAP_READ: GPUBufferUsage.MAP_READ,
        MAP_WRITE: GPUBufferUsage.MAP_WRITE,
        MAP_MODE_READ: GPUMapMode.READ,
      };
      // TODO: stop the animation on unmount
      run(webGPUAsyncDemo)(
        runAnimation,
        device,
        ctx,
        presentationFormat,
        flags,
      );
    };
    initWebGPU();
    return () => {
      runAnimation.value = false;
    };
  });
  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
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
});
