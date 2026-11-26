import React, { useEffect, useRef, useState } from "react";
import { Button, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Needs production JS: in development, the full-window DebuggingOverlay hides the bug.
const MOUNT_DELAY = 1000;

const OverlayCanvas = ({ onLog }: { onLog: (message: string) => void }) => {
  const ref = useRef<CanvasRef>(null);
  const [overlay, setOverlay] = useState(false);

  useEffect(() => {
    let running = true;
    let frame = 0;
    let timer: ReturnType<typeof setTimeout> | undefined;
    (async () => {
      const { device, format } = await initGPU(onLog);
      const ctx = ref.current!.getContext("webgpu")!;
      ctx.configure({ device, format, alphaMode: "opaque" });
      const loop = () => {
        if (!running) {
          return;
        }
        drawClearFrame(device, ctx, frame++);
        requestAnimationFrame(loop);
      };
      loop();
      // Logged before the overlay mounts, or the log could hide the bug.
      onLog("first frame presented, the overlay mounts in a second");
      timer = setTimeout(() => setOverlay(true), MOUNT_DELAY);
    })();
    return () => {
      running = false;
      clearTimeout(timer);
    };
  }, [onLog]);

  return (
    <View style={diagnosticStyles.canvas}>
      <Canvas
        ref={ref}
        style={diagnosticStyles.canvas}
        android={{ backingView: "SurfaceView" }}
      />
      {overlay ? (
        <View style={styles.overlay}>
          <Text style={styles.overlayText}>Overlay</Text>
        </View>
      ) : null}
    </View>
  );
};

export const SurfaceViewOverlay = () => {
  const { log, append } = useDiagnosticLog();
  const [mounted, setMounted] = useState(false);

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          Mounts a SurfaceView canvas, and a yellow overlay over it a second
          after its first frame: the overlay must show. On a broken build
          (Android, production JS) it is cropped away.
        </Text>
        <Button
          title={mounted ? "Unmount canvas" : "Mount canvas"}
          onPress={() => setMounted((value) => !value)}
        />
      </View>
      <View style={diagnosticStyles.log}>
        {log.map((line, i) => (
          <Text key={i} style={diagnosticStyles.logLine}>
            {line}
          </Text>
        ))}
      </View>
      {mounted ? <OverlayCanvas onLog={append} /> : null}
    </View>
  );
};

const styles = {
  overlay: {
    position: "absolute" as const,
    left: 32,
    right: 32,
    top: 32,
    padding: 24,
    alignItems: "center" as const,
    backgroundColor: "#ffd60a",
  },
  overlayText: {
    fontSize: 18,
    fontWeight: "600" as const,
    color: "#111",
  },
};
