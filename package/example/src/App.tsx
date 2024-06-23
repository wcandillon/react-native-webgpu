import * as React from "react";
import { StyleSheet, View } from "react-native";
// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-expect-error
import { WebGPUView, gpu } from "react-native-webgpu";

(gpu as GPU)
  .requestAdapter({ powerPreference: "high-performance" })
  .then((a) => {
    console.log({ a });
  });
// console.log({ gpu: gpu.gpu });

// eslint-disable-next-line import/no-default-export
export default function App() {
  return (
    <View style={styles.container}>
      <WebGPUView color="#32a852" style={styles.box} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
  },
  box: {
    width: 60,
    height: 60,
    marginVertical: 20,
  },
});
