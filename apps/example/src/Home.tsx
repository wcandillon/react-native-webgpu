import * as React from "react";
import { Platform, ScrollView, StyleSheet, Text, View } from "react-native";
import { useNavigation } from "@react-navigation/native";
import { RectButton } from "react-native-gesture-handler";
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
    screen: "ThreeJS",
    title: "☘️ Three.js",
  },
  {
    screen: "Cube",
    title: "🧊 Cube",
  },
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
    title: "🚦 Deferred Rendeering",
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
] as const;

const styles = StyleSheet.create({
  container: {},
  content: {
    paddingBottom: 32,
  },
  thumbnail: {
    backgroundColor: "white",
    padding: 32,
    borderBottomWidth: StyleSheet.hairlineWidth,
  },
  title: {},
});

export const Home = () => {
  const { navigate } =
    useNavigation<StackNavigationProp<Routes, "Home">>();
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {examples.map((thumbnail) => (
        <RectButton
          key={thumbnail.screen}
          onPress={() => navigate(thumbnail.screen)}
        >
          <View style={styles.thumbnail}>
            <Text style={styles.title}>{thumbnail.title}</Text>
          </View>
        </RectButton>
      ))}
    </ScrollView>
  );
};
