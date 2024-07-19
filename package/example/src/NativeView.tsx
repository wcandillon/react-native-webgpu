import React, { useEffect } from "react";
import { View } from "react-native";
import { gpu, WebGPUView } from "react-native-webgpu";

export const NativeView = () => {
  useEffect(() => {
  }, []);
  return <View style={{flex: 1, justifyContent: "center",
    alignItems: "center",}}>
    <WebGPUView style={{
      width: 200,
      height: 200,
      justifyContent: "center",
      alignItems: "center",
    }} />
  </View>;
};
