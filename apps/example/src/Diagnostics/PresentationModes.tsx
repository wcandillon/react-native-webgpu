import React, { useEffect, useRef, useState } from "react";
import {
  Button,
  Platform,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type {
  AndroidCanvasProps,
  CanvasRef,
  RNCanvasContext,
} from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";
import type { SharedValue } from "react-native-reanimated";
import { useSharedValue } from "react-native-reanimated";
import { scheduleOnUI } from "react-native-worklets";

import {
  diagnosticStyles,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// The two ways a canvas presents, side by side, drawn by one loop on the UI
// runtime: the left canvas presents direct (the default), the right one is
// composited. Every frame clears both to the other of two colors, red then
// blue, so that a canvas shown at every frame flickers (a purple blend to the
// eye), while a canvas shown at every other frame holds one color. On
// Android's TextureView, the default backing view, the direct canvas holds a
// color: a frame presented from the main thread's Choreographer callback is
// taken only after the main thread's draw, and the next one replaces it before
// (#542, #613). The composited one flickers: the present thread hands the view
// its next frame once it drew the previous one. On a SurfaceView, and on iOS,
// both flicker. "pause" stops the loop with a frame in each canvas: the two
// hold their last frame, and the composited one shows it again after the app
// went to the background and back, on Android, where the window goes with it.
const alternatingClears = (
  running: SharedValue<boolean>,
  device: GPUDevice,
  direct: RNCanvasContext,
  composited: RNCanvasContext,
  format: GPUTextureFormat,
) => {
  "worklet";
  direct.configure({ device, format, alphaMode: "opaque" });
  composited.configure({ device, format, alphaMode: "opaque" });
  let frame = 0;
  const clear = (context: RNCanvasContext, color: GPUColor) => {
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: color,
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    pass.end();
    device.queue.submit([encoder.finish()]);
    context.present();
  };
  const tick = () => {
    if (!running.value) {
      return;
    }
    const color: GPUColor = frame % 2 === 0 ? [1, 0, 0, 1] : [0, 0, 1, 1];
    frame++;
    clear(direct, color);
    clear(composited, color);
    requestAnimationFrame(tick);
  };
  tick();
};

// The backing views a window-backed canvas can have (a HardwareBufferView
// always composites, its own way).
type BackingView = "TextureView" | "SurfaceView";
const BACKING_VIEWS: BackingView[] = ["TextureView", "SurfaceView"];

// The `android` prop for the backing view, opaque: the colors are what the
// screen is about.
const androidProps = (
  backingView: BackingView,
): AndroidCanvasProps | undefined => {
  if (Platform.OS !== "android") {
    return undefined;
  }
  return backingView === "SurfaceView"
    ? { backingView: "SurfaceView", opaque: true }
    : { backingView: "TextureView", opaque: true };
};

export const PresentationModes = () => {
  const directRef = useRef<CanvasRef>(null);
  const compositedRef = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const running = useSharedValue(true);
  const [backingView, setBackingView] = useState<BackingView>("TextureView");
  const [paused, setPaused] = useState(false);
  // The loop restarts with the canvases: picking another backing view
  // remounts them (the backing view is their key).
  const [generation, setGeneration] = useState(0);

  useEffect(() => {
    running.value = true;
    (async () => {
      const { device, format } = await initGPU(append);
      const direct = directRef.current?.getContext("webgpu");
      const composited = compositedRef.current?.getContext("webgpu");
      if (!direct || !composited) {
        append("no canvas context");
        return;
      }
      append(`loop on the UI runtime, ${backingView}`);
      scheduleOnUI(
        alternatingClears,
        running,
        device,
        direct,
        composited,
        format,
      );
    })();
    return () => {
      running.value = false;
    };
  }, [append, backingView, generation, running]);

  const android = androidProps(backingView);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Left: direct (the default). Right: composited. One loop on the UI
          runtime clears both to red, then blue, every frame. A canvas shown at
          every frame flickers to purple; one shown at every other frame holds a
          color. On Android&apos;s TextureView the direct canvas holds a color
          (#542, #613); the composited one flickers. Pause, then send the app to
          the background and back: on Android the composited canvas shows its
          last frame again, the direct one needs a new frame.
        </Text>
        <View style={styles.buttons}>
          {Platform.OS === "android" &&
            BACKING_VIEWS.map((value) => (
              <Button
                key={value}
                testID={`view-${value}`}
                title={value}
                onPress={() => setBackingView(value)}
              />
            ))}
          <Button
            testID="toggle-loop"
            title={paused ? "resume" : "pause"}
            onPress={() => {
              if (paused) {
                setGeneration((value) => value + 1);
              } else {
                running.value = false;
              }
              setPaused((value) => !value);
            }}
          />
        </View>
      </View>
      <View style={styles.stage}>
        <View style={styles.half}>
          <Text style={styles.label}>direct</Text>
          <Canvas
            key={`direct-${backingView}-${generation}`}
            ref={directRef}
            style={styles.canvas}
            presentation="direct"
            android={android}
          />
        </View>
        <View style={styles.half}>
          <Text style={styles.label}>composited</Text>
          <Canvas
            key={`composited-${backingView}-${generation}`}
            ref={compositedRef}
            style={styles.canvas}
            presentation="composited"
            android={android}
          />
        </View>
      </View>
      <ScrollView style={diagnosticStyles.log}>
        {log.map((line, i) => (
          <Text key={i} style={diagnosticStyles.logLine}>
            {line}
          </Text>
        ))}
      </ScrollView>
    </View>
  );
};

const styles = StyleSheet.create({
  buttons: { flexDirection: "row", flexWrap: "wrap", gap: 8 },
  stage: { flex: 1, flexDirection: "row", gap: 8, padding: 8 },
  half: { flex: 1 },
  label: { textAlign: "center", paddingBottom: 4 },
  canvas: { flex: 1 },
});
