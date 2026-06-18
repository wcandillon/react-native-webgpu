import React, { useRef, useState } from "react";
import { PixelRatio, ScrollView, StyleSheet, Text, View } from "react-native";
import { Canvas, useCanvasRef } from "react-native-webgpu";

import { useElementCapture } from "./useElementCapture";

const ROWS = Array.from({ length: 40 }, (_, i) => i);
const COLORS = ["#1e293b", "#334155"];

// "HTML in Canvas" demo with a native ScrollView. Scrolling changes the view
// every frame natively (no React re-render), so capture-on-change here means
// "capture while scrolling": each onScroll keeps a short activity window open,
// during which we re-capture the live view every frame. When the scroll settles
// we stop capturing and just keep animating the shader on the cached texture.
export function LayoutSubtreeScroll() {
  const ref = useCanvasRef();
  const contentRef = useRef<View>(null);
  const contentSizeRef = useRef<{ w: number; h: number } | null>(null);
  const [error, setError] = useState<string | null>(null);

  // Keep capturing for a short tail after each scroll event so momentum and
  // settling are covered (we capture the live view at render time, so the event
  // only signals activity, it doesn't carry the scroll position).
  const dirtyUntilRef = useRef(0);
  const shouldCaptureRef = useRef(() => Date.now() < dirtyUntilRef.current);

  useElementCapture({
    canvasRef: ref,
    contentRef,
    contentSizeRef,
    shouldCaptureRef,
    setError,
  });

  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={StyleSheet.absoluteFill} layoutSubtree>
        <View
          ref={contentRef}
          style={styles.content}
          collapsable={false}
          onLayout={(e) => {
            const { width: w, height: h } = e.nativeEvent.layout;
            contentSizeRef.current = {
              w: Math.max(1, Math.round(w * PixelRatio.get())),
              h: Math.max(1, Math.round(h * PixelRatio.get())),
            };
          }}
        >
          <ScrollView
            scrollEventThrottle={16}
            onScroll={() => {
              dirtyUntilRef.current = Date.now() + 200;
            }}
          >
            {ROWS.map((i) => (
              <View
                key={i}
                style={[styles.row, { backgroundColor: COLORS[i % 2] }]}
              >
                <Text style={styles.rowText}>Scrolling row #{i}</Text>
              </View>
            ))}
          </ScrollView>
        </View>
      </Canvas>
      {error ? (
        <View style={styles.errorOverlay} pointerEvents="none">
          <Text style={styles.errorText}>{error}</Text>
        </View>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1 },
  content: { flex: 1, backgroundColor: "#0f172a" },
  row: {
    height: 72,
    justifyContent: "center",
    paddingHorizontal: 24,
  },
  rowText: { color: "#f8fafc", fontSize: 20, fontWeight: "600" },
  errorOverlay: {
    ...StyleSheet.absoluteFillObject,
    padding: 16,
    justifyContent: "flex-end",
  },
  errorText: { color: "#f87171", fontSize: 13 },
});
