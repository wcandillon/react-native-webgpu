import React, { useEffect, useMemo, useState } from "react";
import { StyleSheet, Text, View } from "react-native";
import type { CameraDevice } from "react-native-vision-camera";
import {
  useCamera,
  useCameraDevice,
  useCameraPermission,
  useFrameOutput,
} from "react-native-vision-camera";

// Exercises passing a WebGPU object into a Vision Camera frame processor.
//
// `useFrameOutput`'s onFrame worklet runs on a Vision Camera worklet runtime
// (created by react-native-vision-camera-worklets). Unlike the other Reanimated
// examples, which pass the GPUDevice as an *argument* to a worklet dispatched
// from the main runtime, here the worklet CAPTURES the GPUDevice in its
// closure, and Vision Camera ships that closure to its own worklet runtime.
// This is the path that depends on the WebGPU custom serializer being correctly
// registered on every worklet runtime.
//
// Requires a physical device with a camera (the iOS Simulator has none) and
// `react-native-vision-camera-worklets` installed.

// The camera + frame processor live in a child component so their hooks only
// run once we actually have a resolved CameraDevice and a GPUDevice. This
// matters because `useCamera`'s `device` prop is required (CameraDevice |
// CameraPosition) and, if given the "back" position string, resolves it
// synchronously during render and throws
//   This device does not have any "back" Cameras!
// whenever the device list is still empty (the first render, before camera
// permission is granted, since iOS reports no devices until then).
interface CameraSceneProps {
  cameraDevice: CameraDevice;
  device: GPUDevice;
}

const CameraScene = ({ cameraDevice, device }: CameraSceneProps) => {
  // Log only once from inside the worklet so the device round-trips the
  // serializer exactly once instead of spamming every frame.
  const logBox = useMemo(() => ({ seen: false }), []);

  const frameOutput = useFrameOutput({
    pixelFormat: "native",
    onFrame: (frame) => {
      "worklet";
      if (!logBox.seen) {
        logBox.seen = true;
        // Actually use the captured WebGPU device so it is genuinely needed in
        // (and serialized into) the Vision Camera worklet runtime.
        const encoder = device.createCommandEncoder();
        console.log(
          "[FrameProcessor] device on VC runtime label=" +
            String(device.label) +
            " encoderCreated=" +
            String(encoder != null),
        );
      }
      frame.dispose();
    },
  });

  useCamera({
    isActive: true,
    device: cameraDevice,
    outputs: [frameOutput],
  });

  return (
    <View style={styles.center}>
      <Text style={styles.text}>
        Frame processor running. Watch the logs: the captured GPUDevice is
        serialized into the Vision Camera worklet runtime and used to create a
        command encoder once frames start arriving.
      </Text>
    </View>
  );
};

export const FrameProcessor = () => {
  const { hasPermission, requestPermission } = useCameraPermission();
  useEffect(() => {
    if (!hasPermission) {
      requestPermission();
    }
  }, [hasPermission, requestPermission]);

  // useCameraDevice returns `undefined` (instead of throwing) while the device
  // list is still empty, so we can gate on it.
  const cameraDevice = useCameraDevice("back");

  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    let cancelled = false;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      const d = await adapter?.requestDevice();
      if (d && !cancelled) {
        setDevice(d);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  if (!hasPermission) {
    return (
      <View style={styles.center}>
        <Text style={styles.text}>Camera permission is required.</Text>
      </View>
    );
  }
  if (cameraDevice == null) {
    return (
      <View style={styles.center}>
        <Text style={styles.text}>Waiting for back camera…</Text>
      </View>
    );
  }
  if (device == null) {
    return (
      <View style={styles.center}>
        <Text style={styles.text}>Waiting for GPU device…</Text>
      </View>
    );
  }
  return <CameraScene cameraDevice={cameraDevice} device={device} />;
};

const styles = StyleSheet.create({
  center: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    padding: 24,
  },
  text: { textAlign: "center", fontSize: 15 },
});
