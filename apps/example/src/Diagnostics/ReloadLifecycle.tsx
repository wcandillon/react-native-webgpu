import React, { useEffect, useRef, useState } from "react";
import { DevSettings, Pressable, StyleSheet, Text, View } from "react-native";

// Repro / regression screen for issue #461 (SIGSEGV in
// NativeObject<RNWebGPU>::installConstructor after an expo-updates OTA apply).
//
// An OTA apply or DevSettings.reload() destroys the JS runtime and recreates
// it in the same process, then WebGPUModule.install() runs again. The
// per-class prototype caches used to be process-static and detected the
// reload by comparing the new jsi::Runtime pointer with the cached one. When
// Hermes hands the new runtime the address of the freed one (common), the
// stale cache was kept and install() touched jsi::Objects owned by the dead
// runtime. Cached prototypes now live in a JSICache that the runtime itself
// owns (NativeState on a hidden global, see cpp/jsi/JSICache.h): they are
// destroyed with their runtime, so a recreated runtime, at the same address
// or not, always starts with an empty cache.
//
// How to use: press the button (repeatedly). Each press starts pending async
// GPU work and reloads the runtime. Every reload that comes back to a working
// app is one successful install on a recreated runtime; a crash right after
// reload (before any screen is shown) is the bug. On re-opening this screen,
// the checks below confirm the new runtime got fresh prototypes: the objects
// created after reload must be instances of the constructors installed on
// this runtime.

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
  line: {
    color: "#f5f5f5",
    lineHeight: 20,
    fontFamily: "Menlo",
    fontSize: 12,
  },
  fail: {
    color: "#ff6b6b",
  },
  pass: {
    color: "#7ee787",
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

type Check = { label: string; ok: boolean };

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

const runChecks = (adapter: GPUAdapter, device: GPUDevice): Check[] => {
  const buffer = device.createBuffer({
    size: 16,
    usage: GPUBufferUsage.COPY_DST,
  });
  const checks: Check[] = [
    { label: "navigator.gpu instanceof GPU", ok: navigator.gpu instanceof GPU },
    {
      label: "adapter instanceof GPUAdapter",
      ok: adapter instanceof GPUAdapter,
    },
    { label: "device instanceof GPUDevice", ok: device instanceof GPUDevice },
    {
      label: "queue instanceof GPUQueue",
      ok: device.queue instanceof GPUQueue,
    },
    { label: "buffer instanceof GPUBuffer", ok: buffer instanceof GPUBuffer },
    {
      label: "Object.getPrototypeOf(device) === GPUDevice.prototype",
      ok: Object.getPrototypeOf(device) === GPUDevice.prototype,
    },
    {
      label: "String(device) === '[object GPUDevice]'",
      ok: Object.prototype.toString.call(device) === "[object GPUDevice]",
    },
  ];
  buffer.destroy();
  return checks;
};

export const ReloadLifecycle = () => {
  const deviceRef = useRef<GPUDevice | null>(null);
  const [status, setStatus] = useState("Requesting adapter…");
  const [checks, setChecks] = useState<Check[]>([]);
  const [error, setError] = useState<string | null>(null);

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
        setChecks(runChecks(adapter, device));
        setStatus("WebGPU is ready on this runtime.");
        // Keep a native callback registered so runtime teardown has to deal
        // with it.
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
    // Not awaited on purpose: the runtime is torn down while these are
    // pending and their callbacks are still bound to the current runtime.
    void createPendingPipeline(device).catch(() => undefined);
    void device.queue.onSubmittedWorkDone().catch(() => undefined);
    DevSettings.reload("react-native-webgpu reload lifecycle");
  };

  const ready = deviceRef.current !== null;
  const allPass = checks.length > 0 && checks.every((c) => c.ok);

  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Runtime Reload Lifecycle (#461)</Text>
        <Text style={styles.line}>{status}</Text>
        {error ? <Text style={[styles.line, styles.fail]}>{error}</Text> : null}
        {checks.map((c) => (
          <Text
            key={c.label}
            style={[styles.line, c.ok ? styles.pass : styles.fail]}
          >
            {c.ok ? "PASS" : "FAIL"} {c.label}
          </Text>
        ))}
        {checks.length > 0 ? (
          <Text style={[styles.line, allPass ? styles.pass : styles.fail]}>
            {allPass
              ? "Prototypes belong to this runtime."
              : "Stale prototype cache detected!"}
          </Text>
        ) : null}
        <Pressable
          accessibilityRole="button"
          disabled={!ready}
          onPress={reload}
          style={[styles.button, !ready ? styles.buttonDisabled : null]}
        >
          <Text style={styles.buttonText}>Start async work and reload</Text>
        </Pressable>
        <Text style={styles.note}>
          Reload recreates the JS runtime in-process and reinstalls WebGPU. A
          crash right after reload, before any screen appears, is the bug. Come
          back to this screen after each reload; all checks must pass.
        </Text>
      </View>
    </View>
  );
};
