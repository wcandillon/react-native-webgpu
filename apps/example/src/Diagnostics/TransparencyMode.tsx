import React, { useEffect, useRef, useState } from "react";
import {
  Button,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import type {
  AndroidBackingView,
  AndroidCanvasProps,
  CanvasRef,
} from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Exercises every Android backing-view combination: opaque toggles in place
// (on a SurfaceView or a TextureView; a HardwareBufferView always composites
// with alpha), while the backing view is created when the Canvas mounts, so
// picking another kind of view remounts it (the kind is its key; the z-order
// options move a mounted SurfaceView in place on Android 11 and later, and
// remount it before). Half-transparent red is cleared over a blue stage with
// a yellow RN overlay on top:
// - opaque: solid red, overlay visible.
// - translucent TextureView (the default): pink, overlay visible.
// - translucent HardwareBufferView (opt-in, Android 10+): pink, overlay
//   visible. Requesting it on an older device falls back to TextureView.
// - translucent SurfaceView: blends against the window background (black),
//   overlay visible only with zOrderOnTop off (the surface sits below it).
// - translucent SurfaceView + zOrderOnTop: pink, overlay hidden underneath.
// - SurfaceView + zOrderMediaOverlay: above the other SurfaceViews below the
//   window; with a single one, no visible difference.
// The loop draws 5 frames a second by default; "every frame" switches it to
// requestAnimationFrame, which on an opaque SurfaceView is the case where the
// canvas votes the display's refresh rate: on an adaptive display, `dumpsys
// SurfaceFlinger` then lists the SurfaceView layer with a requestedFrameRate
// while it draws, and without one a second after the loop stops. "off"
// presents nothing, so a fresh mount shows the stage until its first frame.
// mounted mounts the Canvas afresh. shown hides it and mounts it again while
// hidden, so it is configured before its surface exists, and reveals it.
interface Option {
  label: string;
  backingView: AndroidBackingView;
  zOrderOnTop?: boolean;
  zOrderMediaOverlay?: boolean;
}

const OPTIONS: Option[] = [
  { label: "texture", backingView: "TextureView" },
  { label: "hardware buffer", backingView: "HardwareBufferView" },
  { label: "surface", backingView: "SurfaceView" },
  { label: "surface on top", backingView: "SurfaceView", zOrderOnTop: true },
  {
    label: "surface media overlay",
    backingView: "SurfaceView",
    zOrderMediaOverlay: true,
  },
];

// The `android` prop of an option, with the opacity the switch asks for (a
// HardwareBufferView takes none).
const androidProps = (option: Option, opaque: boolean): AndroidCanvasProps => {
  switch (option.backingView) {
    case "SurfaceView":
      return {
        backingView: "SurfaceView",
        opaque,
        zOrderOnTop: option.zOrderOnTop,
        zOrderMediaOverlay: option.zOrderMediaOverlay,
      };
    case "TextureView":
      return { backingView: "TextureView", opaque };
    case "HardwareBufferView":
      return { backingView: "HardwareBufferView" };
  }
};

const CLEAR_COLOR: GPUColor = [0.5, 0, 0, 0.5];

type Loop = "5 fps" | "every frame" | "off";

const nextLoop = (loop: Loop): Loop => {
  switch (loop) {
    case "5 fps":
      return "every frame";
    case "every frame":
      return "off";
    case "off":
      return "5 fps";
  }
};

export const TransparencyMode = () => {
  const ref = useRef<CanvasRef>(null);
  const { log, append } = useDiagnosticLog();
  const [option, setOption] = useState(OPTIONS[0]);
  const [opaque, setOpaque] = useState(false);
  const [mounted, setMounted] = useState(true);
  const [shown, setShown] = useState(true);
  const [hiddenCount, setHiddenCount] = useState(0);
  const [loop, setLoop] = useState<Loop>("5 fps");
  const [framesPerSecond, setFramesPerSecond] = useState(0);
  const [taps, setTaps] = useState(0);
  // The loop reads the cadence through a ref so that toggling it does not
  // restart the effect: a second initGPU() would configure the context with a
  // new device while the first still owns the window, and Vulkan refuses the
  // surface (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, device lost).
  const loopRef = useRef(loop);
  useEffect(() => {
    loopRef.current = loop;
  }, [loop]);

  // The key of the Canvas: the kind of backing view, plus its z-order where
  // that is read at mount.
  const zOrderAtMount =
    option.backingView === "SurfaceView" && Number(Platform.Version) < 30;
  const backingKey = `${option.backingView}-${
    zOrderAtMount
      ? `${!!option.zOrderOnTop}-${!!option.zOrderMediaOverlay}`
      : ""
  }`;

  // Runs again for each mount of the Canvas: mounted, shown, and the remount
  // that picking another kind of view causes.
  useEffect(() => {
    if (!mounted) {
      return;
    }
    let running = true;
    let frame = 0;
    let readoutStart = performance.now();
    let framesSinceReadout = 0;
    (async () => {
      const { device, format } = await initGPU(append);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "premultiplied" });
      const tick = () => {
        if (!running) {
          return;
        }
        if (loopRef.current !== "off") {
          try {
            drawClearFrame(device, ctx, frame++, CLEAR_COLOR);
          } catch (e) {
            append(`frame threw: ${(e as Error).message}`);
            running = false;
            return;
          }
          framesSinceReadout++;
        }
        const now = performance.now();
        if (now - readoutStart >= 1000) {
          setFramesPerSecond(
            Math.round((framesSinceReadout * 1000) / (now - readoutStart)),
          );
          framesSinceReadout = 0;
          readoutStart = now;
        }
        if (loopRef.current === "every frame") {
          requestAnimationFrame(tick);
        } else {
          setTimeout(tick, 200);
        }
      };
      tick();
    })();
    return () => {
      running = false;
    };
  }, [append, mounted, backingKey, hiddenCount]);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Half-transparent red over blue. HardwareBufferView and TextureView
          keep the yellow RN overlay visible. A SurfaceView on top blends over
          it. Picking another kind of view remounts the Canvas; opaque (and, on
          Android 11 and later, the z-order options) updates it in place.
          mounted remounts it, shown remounts it hidden and reveals it, and with
          the loop off nothing is presented.
        </Text>
        <View style={styles.buttons}>
          {OPTIONS.map((value) => (
            <Button
              key={value.label}
              testID={`view-${value.label.replace(/ /g, "-")}`}
              title={value.label}
              onPress={() => setOption(value)}
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
            testID="toggle-canvas"
            title={`mounted: ${mounted}`}
            onPress={() => setMounted((value) => !value)}
          />
          <Button
            testID="toggle-shown"
            title={`shown: ${shown}`}
            onPress={() => {
              if (shown) {
                setHiddenCount((count) => count + 1);
              }
              setShown(!shown);
            }}
          />
          <Button
            testID="toggle-loop"
            title={`loop: ${loop}`}
            onPress={() => setLoop(nextLoop)}
          />
        </View>
        <Text testID="view-status" style={diagnosticStyles.description}>
          view: {option.label} opaque: {String(opaque)} overlay taps: {taps}
        </Text>
        <Text testID="frame-rate" style={diagnosticStyles.description}>
          presented: {framesPerSecond} fps
        </Text>
      </View>
      <View style={styles.stage}>
        {mounted && (
          <Canvas
            key={`${backingKey}-${hiddenCount}`}
            ref={ref}
            style={[diagnosticStyles.canvas, !shown && styles.hidden]}
            android={androidProps(option, opaque)}
          />
        )}
        <Pressable
          testID="canvas-overlay"
          style={styles.overlay}
          onPress={() => setTaps((t) => t + 1)}
        >
          <Text>overlay (tap me)</Text>
        </Pressable>
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
  stage: { flex: 1, backgroundColor: "blue" },
  hidden: { display: "none" },
  overlay: {
    position: "absolute",
    left: 24,
    right: 24,
    bottom: 40,
    padding: 12,
    alignItems: "center",
    backgroundColor: "yellow",
  },
});
