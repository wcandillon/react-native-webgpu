import * as React from "react";
import { ScrollView, StyleSheet, Text, View } from "react-native";
import { useNavigation } from "@react-navigation/native";
import { RectButton } from "react-native-gesture-handler";
import type { StackNavigationProp } from "@react-navigation/stack";

import type { Routes } from "./Routes";

export const examples = [
  {
    screen: "Cube",
    title: "🧊 Cube",
  },
  {
    screen: "InstancedMesh",
    title: "🐵 Instanced Mesh",
  },
  {
    screen: "Backdrop",
    title: "💃🏿 Backdrop",
  },
  {
    screen: "Helmet",
    title: "⛑️ Helmet",
  },
  {
    screen: "PostProcessing",
    title: "🪄 Post Processing Effects",
  },
  {
    screen: "Fiber",
    title: "👕 Fiber",
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

export const List = () => {
  const { navigate } = useNavigation<StackNavigationProp<Routes, "List">>();
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
