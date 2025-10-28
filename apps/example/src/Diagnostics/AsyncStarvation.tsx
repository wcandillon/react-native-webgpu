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

export const AsyncStarvation = () => {
  const [status, setStatus] = useState("Requesting adapter…");
  const [result, setResult] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    let timerFired = false;

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
          label: "async-starvation-demo",
        });

        if (cancelled) {
          return;
        }

        setStatus(
          "Scheduling a zero-delay timer and createRenderPipelineAsync call…",
        );

        const timerId = setTimeout(() => {
          timerFired = true;
          if (!cancelled) {
            setStatus("Zero-delay timer fired ✅");
          }
        }, 0);

        const start = Date.now();
        try {
          await device.createRenderPipelineAsync({
            layout: "auto",
            vertex: {
              module: device.createShaderModule({
                code: `
                  @vertex
                  fn main(@builtin(vertex_index) vertexIndex : u32) -> @builtin(position) vec4f {
                    var pos = array<vec2f, 3>(
                      vec2f(0.0, 0.5),
                      vec2f(-0.5, -0.5),
                      vec2f(0.5, -0.5)
                    );
                    return vec4f(pos[vertexIndex], 0.0, 1.0);
                  }
                `,
              }),
              entryPoint: "main",
            },
            fragment: {
              module: device.createShaderModule({
                code: `
                  @fragment
                  fn main() -> @location(0) vec4f {
                    return vec4f(1.0, 0.0, 0.0, 1.0);
                  }
                `,
              }),
              entryPoint: "main",
              targets: [{ format: navigator.gpu.getPreferredCanvasFormat() }],
            },
            primitive: { topology: "triangle-list" },
          });
        } finally {
          clearTimeout(timerId);
        }

        if (cancelled) {
          return;
        }

        const elapsed = Date.now() - start;
        if (timerFired) {
          setResult(
            `Timer fired before createRenderPipelineAsync resolved (${elapsed} ms). This is the expected behaviour.`,
          );
        } else {
          setResult(
            `Timer was still pending when createRenderPipelineAsync resolved (${elapsed} ms).\nThis reproduces the busy-loop behaviour described in the review.`,
          );
        }
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
    };
  }, []);

  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Async Runner Starvation</Text>
        <Text style={styles.paragraph}>{status}</Text>
        {result ? <Text style={styles.result}>{result}</Text> : null}
        <Text style={styles.note}>
          Expected: the zero-delay timer fires before the pipeline promise
          resolves (just like on the Web). Observed: the timer is starved until
          the promise resolves, freezing other JS work.
        </Text>
      </View>
    </View>
  );
};
