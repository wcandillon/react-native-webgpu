import React, { useEffect, useRef, useState } from "react";
import { Button, Pressable, StyleSheet, Text, View } from "react-native";
import type {
  AndroidViewProps,
  CanvasProps,
  CanvasRef,
} from "react-native-webgpu";
import { Canvas, useDevice } from "react-native-webgpu";

const OPTIONS: { label: string; props?: AndroidViewProps }[] = [
  { label: "default" },
  { label: "texture", props: { view: "TextureView" } },
  { label: "surface", props: { view: "SurfaceView" } },
  { label: "on top", props: { view: "SurfaceView", zOrderOnTop: true } },
  { label: "translucent", props: { view: "SurfaceView", translucent: true } },
  {
    label: "overlay",
    props: { view: "SurfaceView", zOrderOnTop: true, translucent: true },
  },
];

const ClearCanvas = ({
  device,
  ...canvasProps
}: CanvasProps & {
  device: GPUDevice;
}) => {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    context.configure({
      device,
      format: navigator.gpu.getPreferredCanvasFormat(),
      alphaMode: "premultiplied",
    });
    const frame = () => {
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: [0.5, 0, 0, 0.5],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
    };
    frame();
    const timer = setInterval(frame, 200);
    return () => {
      clearInterval(timer);
      context.unconfigure();
    };
  }, [device]);

  return <Canvas ref={ref} {...canvasProps} style={styles.canvas} />;
};

export const TransparencyMode = () => {
  const { device } = useDevice();
  const [option, setOption] = useState(OPTIONS[0]);
  const [transparent, setTransparent] = useState(true);
  const [mounted, setMounted] = useState(true);
  const [taps, setTaps] = useState(0);

  return (
    <View style={styles.container}>
      <Text style={styles.copy}>
        Half-transparent red over blue. TextureView preserves the yellow RN
        overlay. A translucent SurfaceView on top blends over it. Options update
        the same mounted Canvas; use hide/show to test a fresh mount.
      </Text>
      <View style={styles.buttons}>
        {OPTIONS.map((value) => (
          <Button
            key={value.label}
            testID={`view-${value.label.replace(" ", "-")}`}
            title={value.label}
            onPress={() => setOption(value)}
          />
        ))}
      </View>
      <View style={styles.buttons}>
        <Button
          testID="toggle-transparent"
          title={`transparent: ${transparent}`}
          onPress={() => setTransparent((value) => !value)}
        />
        <Button
          testID="toggle-canvas"
          title={mounted ? "hide canvas" : "show canvas"}
          onPress={() => setMounted((value) => !value)}
        />
      </View>
      <Text testID="view-status" style={styles.copy}>
        view: {option.label} transparent: {String(transparent)} overlay taps:{" "}
        {taps}
      </Text>
      <View style={styles.stage}>
        {mounted && device && (
          <ClearCanvas
            device={device}
            {...option.props}
            transparent={transparent}
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
    </View>
  );
};

const styles = StyleSheet.create({
  container: { flex: 1, padding: 16, gap: 8, paddingTop: 64 },
  copy: { fontSize: 13 },
  buttons: { flexDirection: "row", flexWrap: "wrap", gap: 8 },
  stage: { flex: 1, backgroundColor: "blue" },
  canvas: { flex: 1 },
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
