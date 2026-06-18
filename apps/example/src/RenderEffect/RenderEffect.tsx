import React, { useRef, useState } from "react";
import {
  findNodeHandle,
  NativeModules,
  Platform,
  ScrollView,
  StyleSheet,
  Switch,
  Text,
  View,
} from "react-native";

// Path B PoC (Android): attach a native AGSL compositor shader to a live RN
// view via the RenderEffectModule (View.setRenderEffect + RuntimeShader). Unlike
// the capture path, scrolling re-runs the shader on RenderThread for free, with
// no per-frame readback and no work from JS. iOS is not wired yet (the toggle
// reports "unavailable").
const { RenderEffectModule } = NativeModules as {
  RenderEffectModule?: {
    applyRenderEffect(tag: number, effect: string): Promise<void>;
    clearRenderEffect(tag: number): Promise<void>;
  };
};

const ROWS = Array.from({ length: 40 }, (_, i) => i);
const COLORS = ["#1e293b", "#334155"];

export function RenderEffect() {
  const containerRef = useRef<View>(null);
  const [enabled, setEnabled] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const toggle = async (value: boolean) => {
    setEnabled(value);
    setError(null);
    if (!RenderEffectModule) {
      setError(`RenderEffectModule unavailable on ${Platform.OS}`);
      return;
    }
    const node = findNodeHandle(containerRef.current);
    if (node == null) {
      setError("Could not resolve the native view tag");
      return;
    }
    try {
      if (value) {
        await RenderEffectModule.applyRenderEffect(node, "vignette");
      } else {
        await RenderEffectModule.clearRenderEffect(node);
      }
    } catch (e) {
      setError(String(e));
    }
  };

  return (
    <View style={styles.root}>
      <View ref={containerRef} collapsable={false} style={styles.effected}>
        <ScrollView scrollEventThrottle={16}>
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
      <View style={styles.toolbar}>
        <Text style={styles.toolbarText}>Native RenderEffect (AGSL)</Text>
        <Switch value={enabled} onValueChange={toggle} />
      </View>
      {error ? (
        <View style={styles.errorOverlay} pointerEvents="none">
          <Text style={styles.errorText}>{error}</Text>
        </View>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "#0f172a" },
  effected: { flex: 1 },
  row: {
    height: 72,
    justifyContent: "center",
    paddingHorizontal: 24,
  },
  rowText: { color: "#f8fafc", fontSize: 20, fontWeight: "600" },
  toolbar: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    paddingHorizontal: 24,
    paddingVertical: 16,
    backgroundColor: "#020617",
  },
  toolbarText: { color: "#f8fafc", fontSize: 16, fontWeight: "600" },
  errorOverlay: {
    ...StyleSheet.absoluteFillObject,
    padding: 16,
    justifyContent: "flex-end",
  },
  errorText: { color: "#f87171", fontSize: 13 },
});
