import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, Pressable, StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

// Two rgba16float canvases side by side, toneMapping "extended" on the left
// and "standard" on the right, each a gray ramp from 0 (bottom) to MAX_VALUE
// (top) in canvas values: smooth up to a full-width black line at 1.0, then
// in steps of 0.5, with short ticks at every other whole value. A strip of
// plain SDR white runs beside each canvas, and the buttons mount one canvas
// alone. On an HDR display:
// - extended: the steps above 1.0 are brighter than the white strip, up to
//   the display's headroom (about 2.5 for 8x, the values being sRGB-encoded),
//   and are flat only from there;
// - standard: everything above 1.0 is the white of the strip, with no steps;
// - below the 1.0 line both look the same.
// A screenshot does not capture EDR: look at the device.
const MAX_VALUE = 4;

const shader = /* wgsl */ `
struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vs(@builtin(vertex_index) index: u32) -> VertexOut {
  let corners = array(vec2f(-1, -1), vec2f(3, -1), vec2f(-1, 3));
  var out: VertexOut;
  out.position = vec4f(corners[index], 0, 1);
  out.uv = (corners[index] + 1) * 0.5;
  return out;
}

@fragment
fn fs(in: VertexOut) -> @location(0) vec4f {
  let ramp = in.uv.y * ${MAX_VALUE}.0;
  // Smooth up to 1.0, then steps of 0.5, so the step where it stops shows.
  let value = select(floor(ramp * 2) / 2, ramp, ramp <= 1);
  let line = abs(ramp - round(ramp)) < fwidth(ramp) * 2;
  let marker = line && round(ramp) == 1;
  let tick = line && round(ramp) > 1 && in.uv.x < 0.25;
  if (marker || tick) {
    return vec4f(0, 0, 0, 1);
  }
  return vec4f(value, value, value, 1);
}
`;

const RampCanvas = ({ mode }: { mode: GPUCanvasToneMappingMode }) => {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    let cancelled = false;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter?.requestDevice();
      const context = ref.current?.getContext("webgpu");
      if (cancelled || !device || !context) {
        return;
      }
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();
      const format: GPUTextureFormat = "rgba16float";
      context.configure({
        device,
        format,
        alphaMode: "opaque",
        toneMapping: { mode },
      });
      const module = device.createShaderModule({ code: shader });
      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: { module, entryPoint: "vs" },
        fragment: { module, entryPoint: "fs", targets: [{ format }] },
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
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
  }, [mode]);
  return (
    <View style={styles.column}>
      <Text style={styles.title}>{mode}</Text>
      <View style={styles.pair}>
        <Canvas ref={ref} toneMapping={{ mode }} style={styles.canvas} />
        {/* Plain SDR white, the reference for the part above 1.0. */}
        <View style={styles.sdrWhite} />
      </View>
    </View>
  );
};

type Shown = "both" | "extended" | "standard";

export const ToneMapping = () => {
  const [shown, setShown] = useState<Shown>("both");
  return (
    <View style={styles.container}>
      <View style={styles.buttons}>
        {(["both", "extended", "standard"] as const).map((option) => (
          <Pressable
            key={option}
            onPress={() => setShown(option)}
            style={[styles.button, shown === option && styles.buttonActive]}
          >
            <Text style={styles.buttonText}>{option}</Text>
          </Pressable>
        ))}
      </View>
      <View style={styles.row} key={shown}>
        {shown !== "standard" ? (
          <RampCanvas mode="extended" />
        ) : (
          <View style={styles.column} />
        )}
        <View style={styles.scale}>
          {Array.from({ length: MAX_VALUE + 1 }, (_, value) => (
            <Text
              key={value}
              style={[
                styles.label,
                { bottom: `${(value / MAX_VALUE) * 100}%` },
              ]}
            >
              {value.toFixed(1)}
            </Text>
          ))}
        </View>
        {shown !== "extended" ? (
          <RampCanvas mode="standard" />
        ) : (
          <View style={styles.column} />
        )}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: "black",
  },
  row: {
    flex: 1,
    flexDirection: "row",
  },
  column: {
    flex: 1,
  },
  title: {
    color: "white",
    textAlign: "center",
    marginBottom: 8,
  },
  pair: {
    flex: 1,
    flexDirection: "row",
  },
  canvas: {
    flex: 1,
  },
  sdrWhite: {
    width: 12,
    marginTop: "auto",
    height: "75%",
    backgroundColor: "white",
  },
  buttons: {
    flexDirection: "row",
    justifyContent: "center",
    gap: 8,
    marginBottom: 8,
  },
  button: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 6,
    backgroundColor: "#333",
  },
  buttonActive: {
    backgroundColor: "#06c",
  },
  buttonText: {
    color: "white",
  },
  scale: {
    width: 40,
    marginTop: 28,
  },
  label: {
    position: "absolute",
    width: "100%",
    textAlign: "center",
    color: "white",
    fontSize: 12,
    transform: [{ translateY: 7 }],
  },
});
