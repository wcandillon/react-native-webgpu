import React, { useEffect, useMemo, useRef, useState } from "react";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import {
  Gesture,
  GestureDetector,
  RectButton,
} from "react-native-gesture-handler";
import Animated, {
  useAnimatedStyle,
  useSharedValue,
} from "react-native-reanimated";
import type { SharedValue } from "react-native-reanimated";
import { runOnUI } from "react-native-worklets";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import { Canvas, useDevice } from "react-native-webgpu";

type PresentMode = "fifo" | "mailbox";

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const AnimatedView = Animated.View as any;

const markerWGSL = /* wgsl */ `
@vertex
fn vertexMain(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
  let positions = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f(3.0, -1.0),
    vec2f(-1.0, 3.0),
  );
  return vec4f(positions[index], 0.0, 1.0);
}

@fragment
fn fragmentMain() -> @location(0) vec4f {
  return vec4f(0.20, 0.82, 1.0, 1.0);
}
`;

const renderMarker = (
  running: SharedValue<boolean>,
  x: SharedValue<number>,
  y: SharedValue<number>,
  device: GPUDevice,
  context: RNCanvasContext,
  format: GPUTextureFormat,
  presentMode: PresentMode,
  width: number,
  height: number,
  pixelRatio: number,
) => {
  "worklet";

  context.configure({
    device,
    format,
    alphaMode: "premultiplied",
    presentMode,
  });

  const module = device.createShaderModule({ code: markerWGSL });
  const pipeline = device.createRenderPipeline({
    layout: "auto",
    vertex: { module, entryPoint: "vertexMain" },
    fragment: {
      module,
      entryPoint: "fragmentMain",
      targets: [{ format }],
    },
  });
  const markerSize = 44 * pixelRatio;

  const frame = () => {
    if (!running.value) {
      return;
    }

    const commandEncoder = device.createCommandEncoder();
    const pass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [0.035, 0.055, 0.095, 1],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    const markerX = Math.min(
      Math.max(x.value * pixelRatio - markerSize / 2, 0),
      width - markerSize,
    );
    const markerY = Math.min(
      Math.max(y.value * pixelRatio - markerSize / 2, 0),
      height - markerSize,
    );
    pass.setPipeline(pipeline);
    pass.setViewport(markerX, markerY, markerSize, markerSize, 0, 1);
    pass.draw(3);
    pass.end();
    device.queue.submit([commandEncoder.finish()]);
    context.present();

    if (running.value) {
      requestAnimationFrame(frame);
    }
  };

  frame();
};

interface PresentModeCanvasProps {
  device: GPUDevice;
  mode: PresentMode;
  x: SharedValue<number>;
  y: SharedValue<number>;
}

const PresentModeCanvas = ({ device, mode, x, y }: PresentModeCanvasProps) => {
  const ref = useRef<CanvasRef>(null);
  const running = useSharedValue(true);

  useEffect(() => {
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return undefined;
    }

    const canvas = context.canvas as HTMLCanvasElement;
    const pixelRatio = PixelRatio.get();
    canvas.width = canvas.clientWidth * pixelRatio;
    canvas.height = canvas.clientHeight * pixelRatio;
    if (x.value < 0 || y.value < 0) {
      x.value = canvas.clientWidth / 2;
      y.value = canvas.clientHeight / 2;
    }
    running.value = true;

    runOnUI(renderMarker)(
      running,
      x,
      y,
      device,
      context,
      navigator.gpu.getPreferredCanvasFormat(),
      mode,
      canvas.width,
      canvas.height,
      pixelRatio,
    );

    return () => {
      running.value = false;
    };
  }, [device, mode, running, x, y]);

  return <Canvas ref={ref} style={styles.canvas} />;
};

export const PresentModeExample = () => {
  const { device } = useDevice();
  const [mode, setMode] = useState<PresentMode>("fifo");
  const x = useSharedValue(-1);
  const y = useSharedValue(-1);
  const gesture = useMemo(
    () =>
      Gesture.Pan()
        .onBegin((event) => {
          x.value = event.x;
          y.value = event.y;
        })
        .onUpdate((event) => {
          x.value = event.x;
          y.value = event.y;
        }),
    [x, y],
  );
  const uiMarkerStyle = useAnimatedStyle(() => ({
    transform: [{ translateX: x.value - 24 }, { translateY: y.value - 24 }],
  }));

  return (
    <View style={styles.container}>
      <View style={styles.controls}>
        <Text style={styles.heading}>Presentation mode</Text>
        <Text style={styles.description}>
          Drag the white ring. The cyan marker is presented by WebGPU from the
          same input. Mailbox favors the newest pending frame; FIFO preserves
          queued frames.
        </Text>
        <View style={styles.modeRow}>
          {(["fifo", "mailbox"] as const).map((presentMode) => {
            const selected = mode === presentMode;
            return (
              <RectButton
                key={presentMode}
                accessibilityRole="button"
                accessibilityState={{ selected }}
                onPress={() => setMode(presentMode)}
                style={[
                  styles.modeButton,
                  selected && styles.modeButtonSelected,
                ]}
              >
                <Text
                  style={[styles.modeText, selected && styles.modeTextSelected]}
                >
                  {presentMode.toUpperCase()}
                </Text>
              </RectButton>
            );
          })}
        </View>
      </View>
      <GestureDetector gesture={gesture}>
        <View style={styles.stage}>
          {device ? (
            <PresentModeCanvas
              key={mode}
              device={device}
              mode={mode}
              x={x}
              y={y}
            />
          ) : null}
          <AnimatedView
            pointerEvents="none"
            style={[styles.uiMarker, uiMarkerStyle]}
          />
        </View>
      </GestureDetector>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#09101d",
  },
  controls: {
    gap: 10,
    padding: 16,
    backgroundColor: "#ffffff",
  },
  heading: {
    color: "#172033",
    fontSize: 18,
    fontWeight: "700",
  },
  description: {
    color: "#4b5568",
    lineHeight: 20,
  },
  modeRow: {
    flexDirection: "row",
    gap: 8,
  },
  modeButton: {
    minWidth: 112,
    alignItems: "center",
    borderRadius: 8,
    backgroundColor: "#e8edf5",
    paddingHorizontal: 16,
    paddingVertical: 12,
  },
  modeButtonSelected: {
    backgroundColor: "#1677ff",
  },
  modeText: {
    color: "#344054",
    fontWeight: "700",
  },
  modeTextSelected: {
    color: "#ffffff",
  },
  stage: {
    flex: 1,
  },
  canvas: {
    ...StyleSheet.absoluteFillObject,
  },
  uiMarker: {
    position: "absolute",
    left: 0,
    top: 0,
    width: 48,
    height: 48,
    borderWidth: 3,
    borderRadius: 24,
    borderColor: "#ffffff",
  },
});
