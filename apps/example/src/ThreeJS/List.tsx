import * as React from "react";
import {
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import { useNavigation } from "@react-navigation/native";
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
  {
    screen: "Retargeting",
    title: "🕺 Animation Retargeting",
  },
  {
    screen: "ToneMapping",
    title: "🎭 Tone Mapping",
  },
  {
    screen: "Memory",
    title: "🧠 Memory",
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
  // Rows stay white even when macOS uses dark appearance.
  title: { color: "black" },
});

export const List = () => {
  const navigation = useNavigation<StackNavigationProp<Routes, "List">>();
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {examples.map((thumbnail) => (
        <TouchableOpacity
          key={thumbnail.screen}
          onPress={() => navigation.navigate(thumbnail.screen)}
        >
          <View style={styles.thumbnail}>
            <Text style={styles.title}>{thumbnail.title}</Text>
          </View>
        </TouchableOpacity>
      ))}
    </ScrollView>
  );
};
