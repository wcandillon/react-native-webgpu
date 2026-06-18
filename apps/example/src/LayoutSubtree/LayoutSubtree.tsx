import React, { useCallback, useRef, useState } from "react";
import { PixelRatio, StyleSheet, Text, TextInput, View } from "react-native";
import { Canvas, useCanvasRef } from "react-native-webgpu";

import { useElementCapture } from "./useElementCapture";

// "HTML in Canvas" demo: a native <View> child of a <Canvas layoutSubtree> is
// painted into the WebGPU scene via queue.copyElementImageToTexture, with an
// animated shader. Capture-on-change: we only re-capture while the field is
// focused (so the caret blinks and typed text stays live); when idle the cached
// texture is reused and just the shader keeps animating.
export function LayoutSubtree() {
  const ref = useCanvasRef();
  const contentRef = useRef<View>(null);
  const contentSizeRef = useRef<{ w: number; h: number } | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [text, setText] = useState("");

  // Active while the input is focused; a short tail after each keystroke covers
  // the case where focus tracking lags a frame behind a programmatic change.
  const focusedRef = useRef(false);
  const dirtyUntilRef = useRef(0);
  const shouldCaptureRef = useRef(
    () => focusedRef.current || Date.now() < dirtyUntilRef.current,
  );

  useElementCapture({
    canvasRef: ref,
    contentRef,
    contentSizeRef,
    shouldCaptureRef,
    setError,
  });

  const onChangeText = useCallback((value: string) => {
    setText(value);
    dirtyUntilRef.current = performance.now() + 200;
  }, []);

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
          <TextInput
            style={styles.input}
            value={text}
            onChangeText={onChangeText}
            onFocus={() => (focusedRef.current = true)}
            onBlur={() => {
              focusedRef.current = false;
              // One more capture to settle the blurred state.
              dirtyUntilRef.current = Date.now() + 200;
            }}
            placeholder="Tap and type…"
            placeholderTextColor="#64748b"
          />
          <Text style={styles.title}>Hello from a native View</Text>
          <View style={styles.swatch} />
          <Text style={styles.caption}>
            A live TextInput, captured only while focused and sampled as a
            WebGPU texture with an animated shader. Tap the field above to edit
            it.
          </Text>
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
  content: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#1e293b",
    padding: 24,
  },
  title: { color: "#f8fafc", fontSize: 28, fontWeight: "700" },
  swatch: {
    width: 120,
    height: 120,
    borderRadius: 24,
    marginVertical: 24,
    backgroundColor: "#38bdf8",
  },
  input: {
    width: "80%",
    backgroundColor: "#0f172a",
    color: "#f8fafc",
    borderColor: "#334155",
    borderWidth: 1,
    borderRadius: 12,
    paddingHorizontal: 16,
    paddingVertical: 12,
    fontSize: 18,
    marginBottom: 24,
  },
  caption: { color: "#94a3b8", fontSize: 14 },
  errorOverlay: {
    ...StyleSheet.absoluteFillObject,
    padding: 16,
    justifyContent: "flex-end",
  },
  errorText: { color: "#f87171", fontSize: 13 },
});
