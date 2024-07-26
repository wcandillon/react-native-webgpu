import * as React from "react";
import { ScrollView, StyleSheet, Text, View } from "react-native";
import { useNavigation } from "@react-navigation/native";
import { RectButton } from "react-native-gesture-handler";
import type { NativeStackNavigationProp } from "@react-navigation/native-stack";

import type { Routes } from "./Route";

export const examples = [
  {
    screen: "HelloTriangle",
    title: "🔺 Hello Triangle",
  },
  {
    screen: "HelloTriangleMSAA",
    title: "🔺 Hello Triangle MSAA",
  },
  {
    screen: "Cube",
    title: "🧊 Cube",
  },
  {
    screen: "TexturedCube",
    title: "🖼️ Textured Cube",
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
    screen: "ABuffer",
    title: "🫖 A-Buffer",
  },
  {
    screen: "Tests",
    title: "🧪 E2E Tests",
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
    useNavigation<NativeStackNavigationProp<Routes, "Home">>();
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
