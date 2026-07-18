import React, { useEffect, useRef, useState } from "react";
import { DevSettings, Pressable, StyleSheet, Text, View } from "react-native";

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 24,
    justifyContent: "center",
    backgroundColor: "#111",
  },
  card: {
    padding: 20,
    gap: 12,
    borderRadius: 12,
    borderCurve: "continuous",
    backgroundColor: "#1e1e1e",
  },
  title: {
    color: "#f5f5f5",
    fontWeight: "600",
  },
  paragraph: {
    color: "#f5f5f5",
    lineHeight: 20,
  },
  session: {
    color: "#4ade80",
    fontFamily: "monospace",
  },
  error: {
    color: "#ffb347",
    lineHeight: 20,
  },
  button: {
    alignItems: "center",
    padding: 12,
    borderRadius: 8,
    borderCurve: "continuous",
    backgroundColor: "#2563eb",
  },
  buttonDisabled: {
    backgroundColor: "#475569",
  },
  buttonText: {
    color: "white",
    fontWeight: "600",
  },
  note: {
    color: "#aaa",
    lineHeight: 18,
  },
});

const createPendingPipeline = (device: GPUDevice) =>
  device.createComputePipelineAsync({
    label: "reload-lifecycle-pending-pipeline",
    layout: "auto",
    compute: {
      module: device.createShaderModule({
        code: "@compute @workgroup_size(1) fn main() {}",
      }),
      entryPoint: "main",
    },
  });

export const ReloadLifecycle = () => {
  const deviceRef = useRef<GPUDevice | null>(null);
  const [status, setStatus] = useState("Requesting adapter…");
  const [error, setError] = useState<string | null>(null);
  const { sessionId } = RNWebGPU;

  useEffect(() => {
    let cancelled = false;

    const prepare = async () => {
      try {
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
          throw new Error("Failed to acquire a GPU adapter.");
        }

        const device = await adapter.requestDevice({
          label: "reload-lifecycle-probe",
        });
        if (cancelled) {
          return;
        }

        deviceRef.current = device;
        setStatus("WebGPU is ready. Reload can now be triggered.");

        // Keep a native callback registered so runtime teardown has to cancel it.
        void device.lost.catch(() => undefined);
      } catch (cause) {
        if (!cancelled) {
          setError(cause instanceof Error ? cause.message : String(cause));
        }
      }
    };

    void prepare();
    return () => {
      cancelled = true;
      deviceRef.current = null;
    };
  }, []);

  const reload = () => {
    const device = deviceRef.current;
    if (!device) {
      return;
    }

    // Do not await these operations: reload must invalidate their callbacks while
    // they are still associated with the current JavaScript runtime.
    void createPendingPipeline(device).catch(() => undefined);
    void device.queue.onSubmittedWorkDone().catch(() => undefined);
    DevSettings.reload("react-native-webgpu lifecycle regression");
  };

  const ready = deviceRef.current !== null;

  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Runtime Reload Lifecycle</Text>
        <Text style={styles.paragraph}>{status}</Text>
        <Text style={styles.session}>Session: {sessionId}</Text>
        {error ? <Text style={styles.error}>{error}</Text> : null}
        <Pressable
          accessibilityRole="button"
          disabled={!ready}
          onPress={reload}
          style={[styles.button, !ready ? styles.buttonDisabled : null]}
        >
          <Text style={styles.buttonText}>Start async work and reload</Text>
        </Pressable>
        <Text style={styles.note}>
          After reload, open this screen again. A healthy installation shows a
          new session id and reaches the ready state. expo-updates reload uses
          the same native runtime invalidation path.
        </Text>
      </View>
    </View>
  );
};
