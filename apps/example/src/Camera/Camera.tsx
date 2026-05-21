import React, { useEffect } from "react";
import { StyleSheet, Text, View } from "react-native";
import {
  Camera as VisionCamera,
  useCameraDevice,
  useCameraPermission,
} from "react-native-vision-camera";

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "black",
  },
  camera: {
    flex: 1,
  },
  centered: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    padding: 24,
  },
  message: {
    color: "white",
    textAlign: "center",
  },
});

export const Camera = () => {
  const { hasPermission, requestPermission } = useCameraPermission();
  const device = useCameraDevice("back");

  useEffect(() => {
    if (!hasPermission) {
      requestPermission();
    }
  }, [hasPermission, requestPermission]);

  if (!hasPermission) {
    return (
      <View style={styles.centered}>
        <Text style={styles.message}>Requesting camera permission...</Text>
      </View>
    );
  }

  if (device == null) {
    return (
      <View style={styles.centered}>
        <Text style={styles.message}>No camera device available.</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <VisionCamera style={styles.camera} device={device} isActive={true} />
    </View>
  );
};
