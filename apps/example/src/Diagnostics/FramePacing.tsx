import React, { useEffect, useRef, useState } from "react";
import { Button, PixelRatio, StyleSheet, Text, View } from "react-native";
import type {
  AndroidSurfaceType,
  CanvasMode,
  CanvasRef,
} from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import { diagnosticStyles } from "./surfaceLifecycle";

// Measures how the frames a canvas renders reach the screen, for every
// presentation setup: a full-screen fragment shader whose loop count sets the
// GPU cost of a frame is rendered on every requestAnimationFrame, with a bar
// sweeping across so a dropped or repeated frame is visible.
//
// The rendered rate (requestAnimationFrame callbacks completed per second) is
// shown on screen and logged every second as "[pacing] rendered N fps". The
// shown rate comes from the compositor's per-layer frame counts: the app
// window's "VRI-..." layer for a TextureView or HardwareBufferView, the
// "SurfaceView[...](BLAST)" buffer layer for a SurfaceView.
//
//   adb shell dumpsys SurfaceFlinger --timestats -enable
//   adb shell dumpsys SurfaceFlinger --timestats -clear
//   sleep 6
//   adb shell dumpsys SurfaceFlinger --timestats -dump   # totalFrames per layerName
//
// Watch the display mode while measuring (`dumpsys display`,
// renderFrameRate): under GPU load Android's adaptive refresh rate switches
// to a lower mode and requestAnimationFrame follows it. See
// packages/webgpu/docs/Presentation.md for the numbers measured.
const LOADS = [0, 25, 80, 120] as const;
const SURFACE_TYPES: (AndroidSurfaceType | "auto")[] = [
  "auto",
  "HardwareBufferView",
];

const shader = /* wgsl */ `
struct Params {
  loops: u32,
  time: f32,
};
@group(0) @binding(0) var<uniform> params: Params;

@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  const pos = array(vec2f(-1, -1), vec2f(3, -1), vec2f(-1, 3));
  return vec4f(pos[i], 0, 1);
}

@fragment fn fs(@builtin(position) p: vec4f) -> @location(0) vec4f {
  var v = p.xy * 0.001 + vec2f(params.time * 0.1);
  for (var i = 0u; i < params.loops; i++) {
    v = vec2f(sin(v.x * 1.7 + v.y), cos(v.y * 1.3 - v.x));
  }
  // A bar sweeping from left to right once a second.
  let bar = select(0.0, 1.0, abs(p.x * 0.001 - fract(params.time)) < 0.02);
  return vec4f(0.5 + 0.5 * v.x, 0.5 + 0.5 * v.y, bar, 1);
}
`;

interface SceneProps {
  loops: number;
  onRate: (fps: number) => void;
}

const useScene = ({ loops, onRate }: SceneProps) => {
  const ref = useRef<CanvasRef>(null);
  const loopsRef = useRef(loops);
  loopsRef.current = loops;
  useEffect(() => {
    let running = true;
    let frame: number | null = null;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter!.requestDevice();
      const context = ref.current!.getContext("webgpu")!;
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();
      const format = navigator.gpu.getPreferredCanvasFormat();
      context.configure({ device, format, alphaMode: "premultiplied" });
      const module = device.createShaderModule({ code: shader });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module },
        fragment: { module, targets: [{ format }] },
      });
      const params = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: params } }],
      });
      const data = new ArrayBuffer(16);
      const u32 = new Uint32Array(data);
      const f32 = new Float32Array(data);
      const start = performance.now();
      let count = 0;
      let second = start;
      const render = () => {
        if (!running) {
          return;
        }
        const now = performance.now();
        u32[0] = loopsRef.current;
        f32[1] = (now - start) / 1000;
        device.queue.writeBuffer(params, 0, data);
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
        pass.end();
        device.queue.submit([encoder.finish()]);
        context.present();
        count++;
        if (now - second >= 1000) {
          const fps = Math.round((count * 1000) / (now - second));
          console.log(`[pacing] rendered ${fps} fps`);
          onRate(fps);
          count = 0;
          second = now;
        }
        frame = requestAnimationFrame(render);
      };
      frame = requestAnimationFrame(render);
    })();
    return () => {
      running = false;
      if (frame !== null) {
        cancelAnimationFrame(frame);
      }
    };
    // The loop reads loopsRef; only a remount restarts it.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  return ref;
};

interface CanvasSceneProps extends SceneProps {
  opaque: boolean;
  mode: CanvasMode;
  surfaceType: AndroidSurfaceType | "auto";
}

const CanvasScene = ({
  opaque,
  mode,
  surfaceType,
  ...scene
}: CanvasSceneProps) => {
  const ref = useScene(scene);
  return (
    <Canvas
      ref={ref}
      style={diagnosticStyles.canvas}
      opaque={opaque}
      mode={mode}
      android={surfaceType === "auto" ? undefined : { surfaceType }}
    />
  );
};

export const FramePacing = () => {
  const [loops, setLoops] = useState<number>(0);
  const [opaque, setOpaque] = useState(true);
  const [mode, setMode] = useState<CanvasMode>("canvas");
  const [surfaceType, setSurfaceType] = useState<AndroidSurfaceType | "auto">(
    "auto",
  );
  const [rate, setRate] = useState(0);
  // Remount the canvas on every setup change so each measurement starts from
  // a fresh view and render loop.
  const key = `${opaque}-${mode}-${surfaceType}`;
  useEffect(() => setRate(0), [key]);
  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Full-screen shader on every requestAnimationFrame; the loop count sets
          its GPU cost. The rendered rate is shown below, the shown rate is read
          from the compositor (see the source for the adb commands).
        </Text>
        <View style={styles.buttons}>
          {LOADS.map((value) => (
            <Button
              key={value}
              testID={`load-${value}`}
              title={`load ${value}`}
              color={value === loops ? "#444" : undefined}
              onPress={() => setLoops(value)}
            />
          ))}
        </View>
        <View style={styles.buttons}>
          <Button
            testID="toggle-opaque"
            title={`opaque: ${opaque}`}
            onPress={() => setOpaque((value) => !value)}
          />
          <Button
            testID="toggle-mode"
            title={`mode: ${mode}`}
            onPress={() =>
              setMode((value) => (value === "canvas" ? "swapchain" : "canvas"))
            }
          />
          <Button
            testID="toggle-surface-type"
            title={`android: ${surfaceType}`}
            onPress={() =>
              setSurfaceType(
                (value) =>
                  SURFACE_TYPES[
                    (SURFACE_TYPES.indexOf(value) + 1) % SURFACE_TYPES.length
                  ],
              )
            }
          />
        </View>
        <Text testID="pacing-status" style={diagnosticStyles.description}>
          load: {loops} opaque: {String(opaque)} mode: {mode} android:{" "}
          {surfaceType} rendered: {rate} fps
        </Text>
      </View>
      <CanvasScene
        key={key}
        loops={loops}
        opaque={opaque}
        mode={mode}
        surfaceType={surfaceType}
        onRate={setRate}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  buttons: { flexDirection: "row", flexWrap: "wrap", gap: 8 },
});
