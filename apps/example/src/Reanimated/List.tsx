import * as React from "react";
import {
  Platform,
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
    screen: "UIThread",
    title: "🧵 UI Thread",
  },
  {
    screen: "DedicatedThread",
    title: "🔀 Dedicated Thread",
  },
  // Camera worklet runtimes are not part of the macOS runner.
  ...(Platform.OS !== "macos"
    ? ([{ screen: "FrameProcessor", title: "📷 Frame Processor" }] as const)
    : []),
  {
    screen: "AsyncBufferUIThread",
    title: "🧵 Async Buffer (UI)",
  },
  {
    screen: "AsyncBufferDedicatedThread",
    title: "🔀 Async Buffer (Dedicated)",
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
