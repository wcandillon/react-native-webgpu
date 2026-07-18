import * as React from "react";
import { ScrollView, StyleSheet, Text, View } from "react-native";
import { useNavigation } from "@react-navigation/native";
import { RectButton } from "react-native-gesture-handler";
import type { StackNavigationProp } from "@react-navigation/stack";

import type { Routes } from "../Route";

// Sub navigation for the diagnostic screens: each entry reproduces a bug or
// stresses an invariant of the native implementation.
const tests = [
  {
    screen: "AsyncStarvation",
    title: "⚠️ Async Runner Starvation",
  },
  {
    screen: "DeviceLostHang",
    title: "⚠️ Device Lost Hang",
  },
  {
    screen: "WorkletRequestAdapter",
    title: "⚠️ Worklet requestAdapter",
  },
  {
    screen: "ContextEdgeCases",
    title: "⚠️ Context Edge Cases",
  },
  {
    screen: "ViewFormatsUseAfterFree",
    title: "⚠️ viewFormats Use-After-Free",
  },
  {
    screen: "RenderAfterUnmount",
    title: "⚠️ Render After Unmount",
  },
  {
    screen: "BackgroundDetach",
    title: "⚠️ Background Detach",
  },
  {
    screen: "SurfaceChurn",
    title: "⚠️ Surface Churn",
  },
] as const;

export const DiagnosticsList = () => {
  const { navigate } =
    useNavigation<StackNavigationProp<Routes, "Diagnostics">>();
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {tests.map((test) => (
        <RectButton key={test.screen} onPress={() => navigate(test.screen)}>
          <View style={styles.thumbnail}>
            <Text style={styles.title}>{test.title}</Text>
          </View>
        </RectButton>
      ))}
    </ScrollView>
  );
};

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
  title: {},
});
