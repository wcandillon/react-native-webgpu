import React, { useEffect, useState } from "react";
import { StyleSheet, Text, View } from "react-native";
import { runOnJS, runOnUI } from "react-native-reanimated";
// Importing from react-native-wgpu pulls in its barrel module (src/main),
// whose top-level code runs WebGPUModule.install() and then patches
// navigator.gpu / global.createImageBitmap from the native `RNWebGPU` global.
//
// That global is only fully populated on the MAIN JS runtime. On a secondary
// worklet runtime (the Reanimated UI runtime here, or a Vision Camera frame
// processor) `RNWebGPU` is undefined or a stripped-down stub. Referencing the
// `Canvas` import inside the UI-thread worklet below forces the barrel to be
// *required, and therefore evaluated, on the UI runtime*.
//
// Before the guards in src/main/index.tsx, that evaluation threw
//   Cannot read property 'bind' of undefined
// because it eagerly called `RNWebGPU.createImageBitmap.bind(RNWebGPU)` while
// `createImageBitmap` was missing on the stub. A throw at module-evaluation
// time poisons the whole module graph on that runtime, so every downstream
// react-native-wgpu import there silently becomes undefined.
//
// This screen is a smoke test for that fix: if it reaches the "✅" state, the
// package bootstrap survived evaluation on the secondary runtime.
import { Canvas } from "react-native-wgpu";

export const WorkletBootstrap = () => {
  const [status, setStatus] = useState(
    "Scheduling a worklet on the UI runtime…",
  );

  useEffect(() => {
    runOnUI(() => {
      "worklet";
      // Touch the `Canvas` import so Reanimated requires (and evaluates) the
      // react-native-wgpu barrel on THIS runtime. Pre-fix, the line above
      // never runs because evaluation crashes here.
      const canvasResolved = typeof Canvas === "function";
      const navigatorOnUiRuntime =
        typeof globalThis.navigator !== "undefined" &&
        globalThis.navigator != null;
      const gpuOnUiRuntime =
        navigatorOnUiRuntime && globalThis.navigator.gpu != null;
      runOnJS(setStatus)(
        "✅ react-native-wgpu evaluated on the UI runtime without crashing.\n\n" +
          `Canvas import resolved: ${canvasResolved}\n` +
          `navigator present on UI runtime: ${navigatorOnUiRuntime}\n` +
          `navigator.gpu present on UI runtime: ${gpuOnUiRuntime}\n\n` +
          "(navigator.gpu is expected to be absent here: RNWebGPU is only " +
          "fully populated on the main runtime. The point is that the module " +
          "no longer crashes while figuring that out.)",
      );
    })();
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.status}>{status}</Text>
      {/* Rendered on the JS thread so the `Canvas` import is unquestionably
          retained by the bundler. */}
      <Canvas style={styles.hiddenCanvas} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 24,
    justifyContent: "center",
  },
  status: {
    fontSize: 16,
    lineHeight: 24,
  },
  hiddenCanvas: {
    width: 1,
    height: 1,
    opacity: 0,
  },
});
