import React, { useState } from "react";
import { Button, StyleSheet, Text, View } from "react-native";
import { runOnUI, scheduleOnRN } from "react-native-worklets";

// Repro for a crash when the first WebGPU call happens on a worklet runtime
// (Reanimated UI runtime, createWorkletRuntime, Vision Camera frame
// processors, ...). RuntimeContext caches itself on the JS runtime via
// jsi::Runtime::setRuntimeData keyed by a default-constructed (all-zero)
// jsi::UUID. Hermes backs runtimeData with an llvh::DenseMap whose reserved
// empty-bucket marker is exactly that all-zero UUID, so storing or looking up
// that key is undefined behavior. On the main runtime it happens to work
// because the first insert lands in bucket 0; on a worklet runtime
// (react-native-worklets seeds runtimeData with its own UUID first) the
// zero-UUID lookup returns an uninitialized bucket and crashes with
// EXC_BAD_ACCESS inside RuntimeContext::getOrCreate, before Dawn is ever
// asked for an adapter.
export const WorkletRequestAdapter = () => {
  const [log, setLog] = useState<string[]>([]);
  const append = (line: string) => setLog((prev) => [...prev, line]);

  const mainRuntime = async () => {
    const adapter = await navigator.gpu.requestAdapter();
    append(
      `main JS runtime: requestAdapter() -> ${adapter ? "GPUAdapter" : "null"}`,
    );
  };

  const workletRuntime = () => {
    // `gpu` is a WebGPU native object; react-native-webgpu registers a custom
    // worklets serializer, so it legally crosses into the worklet runtime.
    const { gpu } = navigator;
    runOnUI(() => {
      "worklet";
      const promise = gpu.requestAdapter();
      promise.then((adapter) => {
        scheduleOnRN(
          append,
          `UI (worklet) runtime: requestAdapter() -> ${
            adapter ? "GPUAdapter" : "null"
          }`,
        );
      });
    })();
  };

  return (
    <View style={styles.container}>
      <Button title="requestAdapter on main JS runtime" onPress={mainRuntime} />
      <Button
        title="requestAdapter on UI worklet runtime"
        onPress={workletRuntime}
      />
      {log.map((line, i) => (
        <Text key={i} style={styles.log}>
          {line}
        </Text>
      ))}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: "center",
    gap: 12,
    padding: 20,
  },
  log: {
    fontFamily: "Menlo",
    fontSize: 12,
  },
});
