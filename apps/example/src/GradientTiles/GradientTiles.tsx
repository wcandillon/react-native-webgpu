import { useMemo, useState } from "react";
import { Button, StyleSheet, Text, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import { common, d, std } from "typegpu";
import {
  useConfigureContext,
  useFrame,
  useMirroredUniform,
  useRoot,
} from "@typegpu/react";

export function GradientTiles() {
  const root = useRoot();

  const [spanX, setSpanX] = useState(4);
  const [spanY, setSpanY] = useState(4);

  // Mirroring React state on the GPU as a uniform
  const span = useMirroredUniform(d.vec2u, d.vec2u(spanX, spanY));

  const pipeline = useMemo(() => {
    // Defining a full-screen shader
    return root.createRenderPipeline({
      vertex: common.fullScreenTriangle,
      fragment: ({ uv }) => {
        "use gpu";
        const red = std.floor(uv.x * d.f32(span.$.x)) / d.f32(span.$.x);
        const green = std.floor(uv.y * d.f32(span.$.y)) / d.f32(span.$.y);
        return d.vec4f(red, green, 0.5, 1.0);
      },
    });
  }, [root, span]);

  const { ref, ctxRef } = useConfigureContext();

  useFrame(() => {
    const ctx = ctxRef.current;
    if (!ctx) return;

    // Drawing to the canvas each frame
    pipeline.withColorAttachment({ view: ctx }).draw(3);

    ctx.present?.();
  });

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} transparent />
      <View style={style.controls}>
        <View style={style.buttonRow}>
          <Text style={style.spanText}>span x: </Text>
          <Button
            title="➖"
            onPress={() => setSpanX((prev) => Math.max(1, prev - 1))}
          />
          <Button
            title="➕"
            onPress={() => setSpanX((prev) => Math.min(prev + 1, 10))}
          />
        </View>

        <View style={style.buttonRow}>
          <Text style={style.spanText}>span y: </Text>
          <Button
            title="➖"
            onPress={() => setSpanY((prev) => Math.max(1, prev - 1))}
          />
          <Button
            title="➕"
            onPress={() => setSpanY((prev) => Math.min(prev + 1, 10))}
          />
        </View>
      </View>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    aspectRatio: 1,
  },
  spanText: {
    fontSize: 20,
    fontWeight: "bold",
  },
  controls: {
    flex: 1,
    justifyContent: "center",
  },
  buttonRow: {
    flexDirection: "row",
    justifyContent: "center",
    alignItems: "center",
  },
});
