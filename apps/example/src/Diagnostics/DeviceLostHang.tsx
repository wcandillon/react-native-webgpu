import React, { useEffect, useState } from "react";
import { StyleSheet, Text, View } from "react-native";

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 24,
    justifyContent: "center",
    backgroundColor: "#111",
  },
  card: {
    backgroundColor: "#1e1e1e",
    borderRadius: 12,
    padding: 20,
  },
  title: {
    fontSize: 20,
    fontWeight: "600",
    marginBottom: 12,
    color: "#f5f5f5",
  },
  paragraph: {
    color: "#f5f5f5",
    marginBottom: 12,
    lineHeight: 20,
  },
  result: {
    color: "#ffb347",
    marginTop: 12,
    lineHeight: 20,
  },
  note: {
    color: "#aaa",
    marginTop: 16,
    fontSize: 12,
    lineHeight: 18,
  },
});

export const DeviceLostHang = () => {
  const [status, setStatus] = useState("Requesting adapter…");
  const [result, setResult] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    let resolved = false;
    let lossTimerId: ReturnType<typeof setTimeout> | null = null;
    let watchdogId: ReturnType<typeof setTimeout> | null = null;

    const run = async () => {
      try {
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
          if (!cancelled) {
            setResult("Failed to acquire a GPU adapter.");
          }
          return;
        }

        const device = await adapter.requestDevice({
          label: "device-lost-hang-demo",
        });

        if (cancelled) {
          return;
        }

        setStatus(
          "device.lost promise captured. Forcing a synthetic device loss in 100 ms…",
        );

        (device.lost as Promise<GPUDeviceLostInfo>)
          .then((info) => {
            resolved = true;
            if (!cancelled) {
              setResult(
                `device.lost resolved with reason "${info.reason}" and message "${info.message}".`,
              );
            }
          })
          .catch((error) => {
            resolved = true;
            if (!cancelled) {
              setResult(
                `device.lost rejected: ${
                  error instanceof Error ? error.message : String(error)
                }`,
              );
            }
          });

        const forceLoss = (
          device as unknown as { forceLossForTesting?: () => void }
        ).forceLossForTesting;
        if (!forceLoss) {
          setResult(
            "forceLossForTesting is unavailable in this build. Recompile with the test helper to reproduce the issue.",
          );
          return;
        }

        lossTimerId = setTimeout(() => {
          if (cancelled) {
            return;
          }
          setStatus(
            "Calling forceLossForTesting – Dawn will deliver the device lost callback asynchronously…",
          );
          forceLoss.call(device);
        }, 100);

        watchdogId = setTimeout(() => {
          if (!cancelled && !resolved) {
            setResult(
              "device.lost is still pending several seconds after we forced the loss.\nThis demonstrates that callbacks never fire without additional ProcessEvents pumping.",
            );
          }
        }, 2500);
      } catch (error) {
        if (!cancelled) {
          setResult(
            `Encountered an error while running the demo: ${
              error instanceof Error ? error.message : String(error)
            }`,
          );
        }
      }
    };

    run();

    return () => {
      cancelled = true;
      if (lossTimerId !== null) {
        clearTimeout(lossTimerId);
      }
      if (watchdogId !== null) {
        clearTimeout(watchdogId);
      }
    };
  }, []);

  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Device Lost Promise Hang</Text>
        <Text style={styles.paragraph}>{status}</Text>
        {result ? <Text style={styles.result}>{result}</Text> : null}
        <Text style={styles.note}>
          Expected: once forceLossForTesting runs, Dawn invokes the device-lost
          callback, resolving the promise. Observed: the promise remains pending
          indefinitely because the native event loop is no longer pumping
          events.
        </Text>
      </View>
    </View>
  );
};
