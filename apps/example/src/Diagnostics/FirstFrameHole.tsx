import React, { useEffect, useRef, useState } from "react";
import { Button, ScrollView, StyleSheet, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

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
const RED: GPUColor = [1, 0, 0, 1];

interface ProbeCanvasProps {
  configureHidden: boolean;
  lateFirstFrame: boolean;
  append: (line: string) => void;
}

// One mount: the device, then the first frame FIRST_FRAME_DELAY_MS later, or
// LATE_FIRST_FRAME_DELAY_MS with lateFirstFrame. With configureHidden the
// canvas is configured while hidden, before its surface exists, and shown
// afterwards.
const ProbeCanvas = ({
  configureHidden,
  lateFirstFrame,
  append,
}: ProbeCanvasProps) => {
  const ref = useRef<CanvasRef>(null);
  const [hidden, setHidden] = useState(configureHidden);

  useEffect(() => {
    let cancelled = false;
    let timer: ReturnType<typeof setTimeout> | undefined;
    initGPU(append)
      .then(({ device, format }) => {
        const context = ref.current?.getContext("webgpu");
        if (cancelled || !context) {
          return;
        }
        context.configure({ device, format, alphaMode: "opaque" });
        setHidden(false);
        timer = setTimeout(
          () => {
            drawClearFrame(device, context, 0, RED);
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

  return (
    <View style={[styles.fill, hidden ? styles.hidden : null]}>
      <Canvas ref={ref} style={StyleSheet.absoluteFill} />
    </View>
  );
};

export const FirstFrameHole = () => {
  const { log, append } = useDiagnosticLog();
  const [mount, setMount] = useState(0);
  const [configureHidden, setConfigureHidden] = useState(false);
  const [lateFirstFrame, setLateFirstFrame] = useState(false);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          An opaque canvas, a SurfaceView on Android, over a blue stage. Its
          first frame, solid red, is drawn half a second after its device is
          ready, or a second and a half with a late first frame, and the stage
          should show until then: black means the window showed the surface
          before it had a frame.
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
        </View>
      </View>
      <View style={styles.stage}>
        <ProbeCanvas
          key={mount}
          configureHidden={configureHidden}
          lateFirstFrame={lateFirstFrame}
          append={append}
        />
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
  stage: { flex: 1, backgroundColor: "#3498db" },
  fill: { flex: 1 },
  hidden: { display: "none" },
});
