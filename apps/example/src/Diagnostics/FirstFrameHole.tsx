import React, { useEffect, useRef, useState } from "react";
import { Button, ScrollView, StyleSheet, Text, View } from "react-native";
import type {
  AndroidSurfaceType,
  CanvasRef,
  RNCanvasContext,
} from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import type { DiagnosticGPU } from "./surfaceLifecycle";
import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Long enough to see the canvas before its first frame.
const FIRST_FRAME_DELAY_MS = 500;
// Longer than the second a window waits for a first frame before Android 14.
const LATE_FIRST_FRAME_DELAY_MS = 1500;
const REATTACH_STEP_MS = 1500;
const RED: GPUColor = [1, 0, 0, 1];
const MAGENTA: GPUColor = [1, 0, 1, 1];
const STAGE_BLUE = "#3498db";
const SURFACE_TYPES: readonly (AndroidSurfaceType | undefined)[] = [
  undefined,
  "TextureView",
  "HardwareBufferView",
  "SurfaceView",
];

interface ReattachStep {
  away: boolean;
  redraw?: GPUColor;
}

// The reattach cycle's steps, one every REATTACH_STEP_MS, in turn: the canvas
// scrolls away or back, and halfway through a step with a redraw draws a
// frame in its colour, magenta while it is away and red while it shows.
const REATTACH_STEPS: readonly ReattachStep[] = [
  { away: true, redraw: MAGENTA },
  { away: false },
  { away: true },
  { away: false, redraw: RED },
  { away: true },
  { away: false },
];

interface Redraw {
  count: number;
  colour: GPUColor;
}

const NO_REDRAW: Redraw = { count: 0, colour: RED };

interface ProbeCanvasProps {
  configureHidden: boolean;
  lateFirstFrame: boolean;
  surfaceType: AndroidSurfaceType | undefined;
  redraw: Redraw;
  append: (line: string) => void;
}

// One mount: the device, then the first frame FIRST_FRAME_DELAY_MS later, or
// LATE_FIRST_FRAME_DELAY_MS with lateFirstFrame. With configureHidden the
// canvas is configured while hidden, before its surface exists, and shown
// afterwards. Every new redraw draws one more frame, in its colour.
const ProbeCanvas = ({
  configureHidden,
  lateFirstFrame,
  surfaceType,
  redraw,
  append,
}: ProbeCanvasProps) => {
  const ref = useRef<CanvasRef>(null);
  const target = useRef<{
    gpu: DiagnosticGPU;
    context: RNCanvasContext;
  } | null>(null);
  const [hidden, setHidden] = useState(configureHidden);

  useEffect(() => {
    let cancelled = false;
    let timer: ReturnType<typeof setTimeout> | undefined;
    initGPU(append)
      .then((gpu) => {
        const context = ref.current?.getContext("webgpu");
        if (cancelled || !context) {
          return;
        }
        context.configure({
          device: gpu.device,
          format: gpu.format,
          alphaMode: "opaque",
        });
        target.current = { gpu, context };
        setHidden(false);
        timer = setTimeout(
          () => {
            drawClearFrame(gpu.device, context, 0, RED);
            append("first frame presented");
          },
          lateFirstFrame ? LATE_FIRST_FRAME_DELAY_MS : FIRST_FRAME_DELAY_MS,
        );
      })
      .catch((error: Error) => append(`init failed: ${error.message}`));
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, [append, lateFirstFrame]);

  useEffect(() => {
    const { current } = target;
    if (redraw.count === 0 || current === null) {
      return;
    }
    drawClearFrame(current.gpu.device, current.context, 0, redraw.colour);
  }, [redraw]);

  return (
    <View style={[styles.fill, hidden ? styles.hidden : null]}>
      <Canvas
        ref={ref}
        style={StyleSheet.absoluteFill}
        android={surfaceType === undefined ? undefined : { surfaceType }}
      />
    </View>
  );
};

// "reattach" scrolls the canvas out of a row that removes its clipped views
// and back (REATTACH_STEPS), so the same canvas leaves its window and comes
// back, mostly without drawing again: its last frame, red or magenta, should
// show again at once.
export const FirstFrameHole = () => {
  const { log, append } = useDiagnosticLog();
  const [mount, setMount] = useState(0);
  const [configureHidden, setConfigureHidden] = useState(false);
  const [lateFirstFrame, setLateFirstFrame] = useState(false);
  const [surfaceTypeIndex, setSurfaceTypeIndex] = useState(0);
  const [reattach, setReattach] = useState(false);
  const [rowWidth, setRowWidth] = useState(0);
  const [redraw, setRedraw] = useState(NO_REDRAW);
  const row = useRef<ScrollView>(null);
  const surfaceType = SURFACE_TYPES[surfaceTypeIndex];

  useEffect(() => {
    if (!reattach || rowWidth === 0) {
      return;
    }
    let step = 0;
    let redrawTimer: ReturnType<typeof setTimeout> | undefined;
    const timer = setInterval(() => {
      const { away, redraw: colour } =
        REATTACH_STEPS[step % REATTACH_STEPS.length];
      step += 1;
      row.current?.scrollTo({ x: away ? rowWidth : 0, animated: false });
      if (colour !== undefined) {
        redrawTimer = setTimeout(
          () => setRedraw((value) => ({ count: value.count + 1, colour })),
          REATTACH_STEP_MS / 2,
        );
      }
    }, REATTACH_STEP_MS);
    return () => {
      clearInterval(timer);
      clearTimeout(redrawTimer);
    };
  }, [reattach, rowWidth]);

  const canvas = (
    <ProbeCanvas
      key={mount}
      configureHidden={configureHidden}
      lateFirstFrame={lateFirstFrame}
      surfaceType={surfaceType}
      redraw={reattach ? redraw : NO_REDRAW}
      append={append}
    />
  );

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          An opaque canvas, a SurfaceView on Android, over a blue stage. Its
          first frame, solid red, is drawn half a second after its device is
          ready, or a second and a half with a late first frame, and the stage
          should show until then: black means the window showed the surface
          before it had a frame. When it scrolls away and back, its last frame,
          red or magenta, should show again at once.
        </Text>
        <View style={styles.buttons}>
          <Button
            testID="remount"
            title="remount"
            onPress={() => setMount((value) => value + 1)}
          />
          <Button
            testID="toggle-configure-hidden"
            title={`configure while hidden: ${configureHidden}`}
            onPress={() => {
              setConfigureHidden((value) => !value);
              setMount((value) => value + 1);
            }}
          />
          <Button
            testID="toggle-late-first-frame"
            title={`late first frame: ${lateFirstFrame}`}
            onPress={() => {
              setLateFirstFrame((value) => !value);
              setMount((value) => value + 1);
            }}
          />
          <Button
            testID="cycle-surface-type"
            title={`surface: ${surfaceType ?? "auto"}`}
            onPress={() => {
              setSurfaceTypeIndex(
                (value) => (value + 1) % SURFACE_TYPES.length,
              );
              setMount((value) => value + 1);
            }}
          />
          <Button
            testID="toggle-reattach"
            title={`reattach: ${reattach}`}
            onPress={() => {
              setReattach((value) => !value);
              setRedraw(NO_REDRAW);
              setMount((value) => value + 1);
            }}
          />
        </View>
      </View>
      <View style={styles.stage}>
        {reattach ? (
          <ScrollView
            ref={row}
            horizontal
            removeClippedSubviews
            scrollEnabled={false}
            showsHorizontalScrollIndicator={false}
            style={styles.row}
            onLayout={(event) => setRowWidth(event.nativeEvent.layout.width)}
          >
            <View style={[styles.home, { width: rowWidth }]}>
              {rowWidth > 0 ? canvas : null}
            </View>
            <View style={[styles.away, { width: rowWidth }]} />
          </ScrollView>
        ) : (
          canvas
        )}
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
  stage: { flex: 1, backgroundColor: STAGE_BLUE },
  // The row shows its own colour only in the frame between a scroll and the
  // clipping that follows it, when neither cell is attached.
  row: { backgroundColor: "#7f8c8d" },
  home: { backgroundColor: STAGE_BLUE },
  away: { backgroundColor: "#27ae60" },
  fill: { flex: 1 },
  hidden: { display: "none" },
});
