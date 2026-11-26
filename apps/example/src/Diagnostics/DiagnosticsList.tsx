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
    screen: "ReloadLifecycle",
    title: "⚠️ Runtime Reload Lifecycle (#461)",
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
  {
    screen: "TransparencyMode",
    title: "⚠️ Android View Props",
  },
  {
    screen: "SurfaceViewOverlay",
    title: "⚠️ SurfaceView Overlay",
  },
  {
    screen: "DeviceDestroyBeforeDetach",
    title: "⚠️ Device Destroy Before Detach",
  },
] as const;

export const DiagnosticsList = () => {
  const navigation =
    useNavigation<StackNavigationProp<Routes, "Diagnostics">>();
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {tests.map((test) => (
        <TouchableOpacity
          key={test.screen}
          onPress={() => navigation.navigate(test.screen)}
        >
          <View style={styles.thumbnail}>
            <Text style={styles.title}>{test.title}</Text>
          </View>
        </TouchableOpacity>
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
  // Rows stay white even when macOS uses dark appearance.
  title: { color: "black" },
});
