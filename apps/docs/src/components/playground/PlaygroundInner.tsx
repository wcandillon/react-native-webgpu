"use client";

import dynamic from "next/dynamic";
import { View } from "react-native";
import { GPUDeviceProvider } from "react-native-webgpu";

import { HelloTriangleDemo } from "@/demos/HelloTriangleDemo";
import { RotatingCubeDemo } from "@/demos/RotatingCubeDemo";
import { TransparencyDemo } from "@/demos/TransparencyDemo";
import type { PlaygroundDemo } from "./WebGPUPlayground";

const BoidsDemo = dynamic(
  () => import("@/demos/BoidsDemo").then((m) => m.BoidsDemo),
  { ssr: false },
);
const ParticlesDemo = dynamic(
  () => import("@/demos/ParticlesDemo").then((m) => m.ParticlesDemo),
  { ssr: false },
);
const CameraEffectDemo = dynamic(
  () => import("@/demos/CameraEffectDemo").then((m) => m.CameraEffectDemo),
  { ssr: false },
);
const ThreeJsDemo = dynamic(
  () => import("@/demos/ThreeJsDemo").then((m) => m.ThreeJsDemo),
  { ssr: false },
);
const TensorflowDemo = dynamic(
  () => import("@/demos/TensorflowDemo").then((m) => m.TensorflowDemo),
  { ssr: false },
);

interface PlaygroundInnerProps {
  demo: PlaygroundDemo;
  height: number;
  transparent?: boolean;
}

export default function PlaygroundInner({
  demo,
  height,
  transparent,
}: PlaygroundInnerProps) {
  if (demo === "tensorflow") {
    return (
      <View style={{ height, width: "100%", position: "relative" }}>
        <TensorflowDemo />
      </View>
    );
  }

  return (
    <GPUDeviceProvider>
      <View style={{ height, width: "100%", position: "relative" }}>
        {demo === "hello-triangle" ? (
          <HelloTriangleDemo transparent={transparent} />
        ) : null}
        {demo === "rotating-cube" ? <RotatingCubeDemo /> : null}
        {demo === "transparency" ? <TransparencyDemo /> : null}
        {demo === "boids" ? <BoidsDemo /> : null}
        {demo === "particles" ? <ParticlesDemo /> : null}
        {demo === "camera-effect" ? <CameraEffectDemo /> : null}
        {demo === "three-js" ? <ThreeJsDemo /> : null}
      </View>
    </GPUDeviceProvider>
  );
}
