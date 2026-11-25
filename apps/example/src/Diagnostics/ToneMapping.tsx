import React, { useEffect, useRef, useState } from "react";
import { PixelRatio, Pressable, StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas, useDevice } from "react-native-webgpu";

// Canvases with toneMapping "extended" (left) and "standard" (right), each a
// gray ramp from 0 (bottom) to MAX_VALUE (top): smooth up to a black line at
// 1.0, then in steps of 0.5, beside a strip of plain SDR white. On an HDR
// display, with rgba16float, the extended steps above 1.0 are brighter than
// the strip up to the display's headroom, and standard is the strip's white;
// with bgra8unorm both clip at 1.0. Below 1.0 they all look the same.
// A screenshot does not capture EDR: look at the device.
const MAX_VALUE = 4;
const LABEL_HEIGHT = 14;

const MODES = ["extended", "standard"] as const;
const SHOWN = ["both", ...MODES] as const;
const FORMATS = ["rgba16float", "bgra8unorm"] as const;

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

interface RampCanvasProps {
  device: GPUDevice;
  mode: GPUCanvasToneMappingMode;
  format: GPUTextureFormat;
}

const RampCanvas = ({ device, mode, format }: RampCanvasProps) => {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
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
  }, [device, mode, format]);
  return (
    <View style={styles.pair}>
      <Canvas ref={ref} toneMapping={{ mode }} style={styles.canvas} />
      {/* Plain SDR white, the reference for the part above 1.0. */}
      <View style={styles.sdrWhite} />
    </View>
  );
};

const Buttons = <T extends string>({
  options,
  value,
  onChange,
}: {
  options: readonly T[];
  value: T;
  onChange: (value: T) => void;
}) => (
  <View style={styles.buttons}>
    {options.map((option) => (
      <Pressable
        key={option}
        onPress={() => onChange(option)}
        style={[styles.button, value === option && styles.buttonActive]}
      >
        <Text style={styles.buttonText}>{option}</Text>
      </Pressable>
    ))}
  </View>
);

export const ToneMapping = () => {
  const { device } = useDevice();
  const [shown, setShown] = useState<(typeof SHOWN)[number]>("both");
  const [format, setFormat] = useState<(typeof FORMATS)[number]>(FORMATS[0]);
  return (
    <View style={styles.container}>
      <Buttons options={SHOWN} value={shown} onChange={setShown} />
      <Buttons options={FORMATS} value={format} onChange={setFormat} />
      <View style={styles.row}>
        {MODES.map((mode, index) => (
          <React.Fragment key={mode}>
            {index > 0 && <View style={styles.scale} />}
            <Text style={[styles.column, styles.title]}>{mode}</Text>
          </React.Fragment>
        ))}
      </View>
      <View style={[styles.row, styles.ramps]}>
        {MODES.map((mode, index) => (
          <React.Fragment key={mode}>
            {index > 0 && (
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
            )}
            <View style={styles.column}>
              {device && (shown === "both" || shown === mode) && (
                // A new format needs a new canvas: the layer is tagged as the
                // canvas mounts.
                <RampCanvas
                  key={format}
                  device={device}
                  mode={mode}
                  format={format}
                />
              )}
            </View>
          </React.Fragment>
        ))}
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
    flexDirection: "row",
  },
  ramps: {
    flex: 1,
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
    height: `${((MAX_VALUE - 1) / MAX_VALUE) * 100}%`,
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
  },
  label: {
    position: "absolute",
    width: "100%",
    textAlign: "center",
    color: "white",
    fontSize: 12,
    lineHeight: LABEL_HEIGHT,
    transform: [{ translateY: LABEL_HEIGHT / 2 }],
  },
});
