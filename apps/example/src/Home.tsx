import * as React from "react";
import {
  Platform,
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import { CommonActions, useNavigation } from "@react-navigation/native";
import type { StackNavigationProp } from "@react-navigation/stack";

import type { Routes } from "./Route";

export const examples = [
  {
    screen: "Tests",
    title: "🧪 E2E Tests",
  },
  {
    screen: "HelloTriangle",
    title: "🔺 Hello Triangle",
  },
  {
    screen: "HelloTriangleMSAA",
    title: "🔺 Hello Triangle MSAA",
  },
  {
    screen: "ImageBlur",
    title: "🫧 Image Blur",
  },
  {
    screen: "Reanimated",
    title: "🐎 Reanimated",
  },
  // {
  //   screen: "ComputeToys",
  //   title: "🧮 Compute Toys",
  // },
  {
    screen: "ThreeJS",
    title: "☘️ Three.js",
  },
  {
    screen: "Tensorflow",
    title: "🤖 tensorflow.js",
  },
  {
    screen: "Cube",
    title: "🧊 Cube",
  },
  // {
  //   screen: "CanvasAPI",
  //   title: "🧩 Canvas API",
  // },
  {
    screen: "Cubemap",
    title: "🌉 Cubemap",
  },
  {
    screen: "InstancedCube",
    title: "🎲 Instanced Cube",
  },
  {
    screen: "TexturedCube",
    title: "🥷 Textured Cube",
  },
  {
    screen: "FractalCube",
    title: "❄️ Fractal Cube",
  },
  {
    screen: "Particles",
    title: "🌌 Particles",
  },
  {
    screen: "RenderBundles",
    title: "🪐 Render Bundles",
  },
  {
    screen: "OcclusionQuery",
    title: "🟩 Occlusion Query",
  },
  {
    screen: "ReversedZ",
    title: "🟥 Reversed Z",
  },
  {
    screen: "ComputeBoids",
    title: "🐦‍⬛ Compute Boids",
  },
  {
    screen: "MNISTInference",
    title: "1️⃣ MNIST Inference",
  },
  ...(Platform.OS !== "ios"
    ? ([
        {
          screen: "ShadowMapping",
          title: "🐲 Shadow Mapping",
        },
        {
          screen: "SamplerParameters",
          title: "🏁 Sampler Parameters",
        },
      ] as const)
    : []),
  {
    screen: "DeferedRendering",
    title: "🚦 Deferred Rendering",
  },
  {
    screen: "ABuffer",
    title: "🫖 A-Buffer",
  },
  {
    screen: "Wireframe",
    title: "🧬 Wireframe",
  },
  {
    screen: "Resize",
    title: "↔️ Resize",
  },
  {
    screen: "GradientTiles",
    title: "🌈 Gradient Tiles",
  },
  {
    screen: "Diagnostics",
    title: "⚠️ Tests",
  },
  {
    screen: "StorageBufferVertices",
    title: "💾 Storage Buffer Vertices",
  },
  {
    screen: "SharedTextureMemory",
    title: "🎞️ Shared Texture Memory",
  },
  {
    screen: "ImportExternalTexture",
    title: "🎬 Import External Texture",
  },
  // The native camera integration is not linked into the desktop runner.
  ...(Platform.OS !== "macos"
    ? ([
        { screen: "VisionCamera", title: "📷 VisionCamera integration" },
      ] as const)
    : []),
  {
    screen: "SixteenBitTextures",
    title: "🎨 16bits Textures",
  },
  {
    screen: "ViewSnapshot",
    title: "📸 View Snapshot (drawElementImageToTexture)",
  },
  {
    screen: "NativeBufferView",
    title: "🖼️ Native Buffer View",
  },
  {
    screen: "IconBenchmark",
    title: "⏱️ 200 Icons: Skia Canvas vs NativeBufferView",
  },
] as const;

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  content: {
    marginBottom: 32,
  },
  thumbnail: {
    backgroundColor: "white",
    padding: 32,
    borderBottomWidth: StyleSheet.hairlineWidth,
  },
  // Rows stay white even when macOS uses dark appearance.
  title: { color: "black" },
});

// Use React Native touchables so mouse clicks survive returning from a demo.
export const Home = () => {
  const navigation = useNavigation<StackNavigationProp<Routes, "Home">>();
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {examples.map((thumbnail) => (
        <TouchableOpacity
          key={thumbnail.screen}
          onPress={() =>
            navigation.dispatch(CommonActions.navigate(thumbnail.screen))
          }
        >
          <View style={styles.thumbnail}>
            <Text style={styles.title}>{thumbnail.title}</Text>
          </View>
        </TouchableOpacity>
      ))}
    </ScrollView>
  );
};
