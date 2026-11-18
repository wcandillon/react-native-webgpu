import React from "react";
import { Text, View } from "react-native";

// Camera's Nitro bindings are mobile-only; keep them outside the macOS bundle.
export const VisionCamera = () => (
  <View style={{ flex: 1, padding: 24 }}>
    <Text>This camera integration example requires iOS or Android.</Text>
  </View>
);
