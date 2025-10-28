import React, { useEffect, useState } from "react";
import { StyleSheet, Text, View, InteractionManager } from "react-native";

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
  resultSuccess: {
    color: "#4ade80",
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
  const [testPassed, setTestPassed] = useState(false);

  useEffect(() => {
    let cancelled = false;
    let rafFired = false;
    let interactionCompleted = false;
    let rafId: number | null = null;
    let interactionHandle: { cancel: () => void } | null = null;

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
          "Testing async operation impact on React Native event loop…",
        );

        // Use requestAnimationFrame which is more consistent across platforms
        rafId = requestAnimationFrame(() => {
          rafFired = true;
          if (!cancelled) {
            setStatus("Animation frame callback fired ✅");
          }
        });

        // Also test with InteractionManager which is React Native specific
        interactionHandle = InteractionManager.runAfterInteractions(() => {
          interactionCompleted = true;
        });

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
          // Clean up callbacks
          if (rafId !== null) {
            cancelAnimationFrame(rafId);
          }
          if (interactionHandle) {
            interactionHandle.cancel();
          }
        }

        if (cancelled) {
          return;
        }

        const elapsed = Date.now() - start;

        // Check if our callbacks fired during the async operation
        if (rafFired && interactionCompleted) {
          setTestPassed(true);
          setResult(
            `✅ Event loop remained responsive! Animation frame and interactions completed during async operation (${elapsed} ms).\n\nThis indicates the WebGPU async operations are properly non-blocking in React Native.`,
          );
        } else if (rafFired && !interactionCompleted) {
          setTestPassed(true);
          setResult(
            `⚠️ Partial responsiveness: Animation frame fired but interactions were delayed (${elapsed} ms).\n\nThe render loop is working but interaction handling may be affected.`,
          );
        } else {
          setTestPassed(false);
          setResult(
            `❌ Event loop was blocked! Neither animation frames nor interactions could execute during the async operation (${elapsed} ms).\n\nThis indicates potential performance issues with WebGPU async operations in React Native.`,
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
      if (rafId !== null) {
        cancelAnimationFrame(rafId);
      }
      if (interactionHandle) {
        interactionHandle.cancel();
      }
    };
  }, []);

  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.title}>Async Runner Starvation</Text>
        <Text style={styles.paragraph}>{status}</Text>
        {result ? (
          <Text style={testPassed ? styles.resultSuccess : styles.result}>
            {result}
          </Text>
        ) : null}
        <Text style={styles.note}>
          This test verifies that WebGPU async operations don't block React
          Native's event loop. We test both requestAnimationFrame (rendering)
          and InteractionManager (user interactions) to ensure the app remains
          responsive during GPU operations.
        </Text>
      </View>
    </View>
  );
};
