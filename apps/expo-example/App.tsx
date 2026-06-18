import { useEffect, useState } from "react";
import { Platform, StatusBar, StyleSheet, Text as RNText, View } from "react-native";
import { Host, VStack, Text } from "@expo/ui/swift-ui";
import {
  background,
  cornerRadius,
  foregroundColor,
  frame,
  offset,
  padding,
} from "@expo/ui/swift-ui/modifiers";

import { layerEffectShader } from "./modules/render-effect";

// Path B + @expo/ui PoC: the content below is authored in React Native with
// @expo/ui primitives (Host / VStack / Text), which render as genuine SwiftUI.
// Our Metal shader is attached as a custom SwiftUI modifier (layerEffectShader)
// in the SAME SwiftUI tree, so the GPU effect re-applies every frame the
// RN-driven content changes, with no capture and no readback.
const ROWS = [
  "#2563eb",
  "#7c3aed",
  "#0d9488",
  "#db2777",
  "#ea580c",
  "#4f46e5",
  "#16a34a",
];

export default function App() {
  // Drive the content from JS so you can see RN-authored views animating live
  // under the GPU shader. (In production this could be driven natively to avoid
  // per-frame JS; here it proves the RN -> SwiftUI -> shader path end to end.)
  const [t, setT] = useState(0);
  useEffect(() => {
    let raf = 0;
    const start = Date.now();
    const loop = () => {
      setT((Date.now() - start) / 1000);
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
  }, []);

  return (
    <View style={styles.root}>
      <Host style={styles.host} useViewportSizeMeasurement>
        <VStack
          spacing={14}
          modifiers={[layerEffectShader(), frame({ maxWidth: 2000 })]}
        >
          <Text modifiers={[foregroundColor("#f8fafc"), padding({ top: 64 })]}>
            {`LIVE  ${(t % 10000).toFixed(2)}`}
          </Text>

          {ROWS.map((color, i) => {
            const dx = Math.round(Math.sin(t * 1.6 + i * 0.6) * 70);
            return (
              <Text
                key={i}
                modifiers={[
                  foregroundColor("#ffffff"),
                  padding({ horizontal: 24, vertical: 14 }),
                  frame({ maxWidth: 1000 }),
                  background(color),
                  cornerRadius(12),
                  offset({ x: dx, y: 0 }),
                ]}
              >
                {`React Native row #${i}`}
              </Text>
            );
          })}
        </VStack>
      </Host>

      <View style={styles.toolbar}>
        <RNText style={styles.toolbarText}>
          @expo/ui content + Metal layerEffect ({Platform.OS})
        </RNText>
      </View>
      <StatusBar barStyle="light-content" />
    </View>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "#0f172a" },
  host: { flex: 1 },
  toolbar: {
    paddingHorizontal: 24,
    paddingVertical: 16,
    backgroundColor: "#020617",
  },
  toolbarText: { color: "#f8fafc", fontSize: 16, fontWeight: "600" },
});
