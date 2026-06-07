"use client";

import { useEffect, useRef, useState } from "react";
import {
  ActivityIndicator,
  Pressable,
  StyleSheet,
  Text,
  TextInput,
  View,
} from "react-native";

import { initSentimentModel, predictSentiment } from "./sentimentModel";

import type * as Tf from "@tensorflow/tfjs";

type Phase = "loading" | "training" | "ready" | "error";

export function TensorflowDemo() {
  const [phase, setPhase] = useState<Phase>("loading");
  const [backend, setBackend] = useState<string>("");
  const [epoch, setEpoch] = useState(0);
  const [totalEpochs, setTotalEpochs] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [inputText, setInputText] = useState("this is great");
  const [score, setScore] = useState<number | null>(null);
  const [analyzing, setAnalyzing] = useState(false);

  const tfRef = useRef<typeof Tf | null>(null);
  const modelRef = useRef<Tf.LayersModel | null>(null);

  useEffect(() => {
    let cancelled = false;

    (async () => {
      try {
        await import("@/lib/react-native-webgpu");
        await import("@tensorflow/tfjs-backend-webgpu");
        const tfModule = await import("@tensorflow/tfjs");
        const tf = ("default" in tfModule && tfModule.default
          ? tfModule.default
          : tfModule) as typeof import("@tensorflow/tfjs");
        tfRef.current = tf;

        if (cancelled) return;

        setPhase("training");
        const { model, backend: activeBackend } = await initSentimentModel(
          tf,
          (current, total) => {
            if (!cancelled) {
              setEpoch(current);
              setTotalEpochs(total);
            }
          },
        );

        if (cancelled) {
          model.dispose();
          return;
        }

        modelRef.current = model;
        setBackend(activeBackend);
        setPhase("ready");

        const initial = await predictSentiment(tf, model, "this is great");
        if (!cancelled) {
          setScore(initial);
        }
      } catch (e) {
        if (!cancelled) {
          setError(e instanceof Error ? e.message : "Failed to init TensorFlow.js");
          setPhase("error");
        }
      }
    })();

    return () => {
      cancelled = true;
      modelRef.current?.dispose();
      modelRef.current = null;
    };
  }, []);

  const analyze = async () => {
    const tf = tfRef.current;
    const model = modelRef.current;
    if (!tf || !model || !inputText.trim()) return;

    setAnalyzing(true);
    try {
      const value = await predictSentiment(tf, model, inputText.trim());
      setScore(value);
    } finally {
      setAnalyzing(false);
    }
  };

  const trainingProgress =
    totalEpochs > 0 ? Math.round((epoch / totalEpochs) * 100) : 0;

  const sentimentLabel =
    score === null ? null : score > 0.5 ? "Positive" : "Negative";
  const sentimentColor = score === null ? "#94a3b8" : score > 0.5 ? "#16a34a" : "#dc2626";

  return (
    <View style={styles.root}>
      <View style={styles.header}>
        <Text style={styles.title}>Sentiment on WebGPU</Text>
        {backend ? (
          <View style={[styles.badge, backend === "webgpu" && styles.badgeWebgpu]}>
            <Text style={styles.badgeText}>{backend}</Text>
          </View>
        ) : null}
      </View>

      {phase === "loading" || phase === "training" ? (
        <View style={styles.center}>
          <ActivityIndicator size="small" color="#6366f1" />
          <Text style={styles.statusText}>
            {phase === "loading"
              ? "Loading TensorFlow.js…"
              : `Training on GPU (${epoch}/${totalEpochs})…`}
          </Text>
          {phase === "training" ? (
            <View style={styles.progressTrack}>
              <View style={[styles.progressFill, { width: `${trainingProgress}%` }]} />
            </View>
          ) : null}
        </View>
      ) : null}

      {phase === "error" ? (
        <View style={styles.center}>
          <Text style={styles.errorText}>{error}</Text>
        </View>
      ) : null}

      {phase === "ready" ? (
        <>
          <TextInput
            style={styles.input}
            multiline
            value={inputText}
            onChangeText={setInputText}
            placeholder="Type a sentence…"
            placeholderTextColor="#94a3b8"
          />

          <Pressable
            style={[styles.button, analyzing && styles.buttonDisabled]}
            onPress={() => void analyze()}
            disabled={analyzing}
          >
            <Text style={styles.buttonText}>
              {analyzing ? "Running inference…" : "Analyze sentiment"}
            </Text>
          </Pressable>

          {score !== null ? (
            <View style={styles.result}>
              <View style={styles.resultRow}>
                <Text style={styles.resultLabel}>{sentimentLabel}</Text>
                <Text style={styles.scoreText}>{score.toFixed(3)}</Text>
              </View>
              <View style={styles.barTrack}>
                <View
                  style={[
                    styles.barFill,
                    { width: `${Math.round(score * 100)}%`, backgroundColor: sentimentColor },
                  ]}
                />
              </View>
              <Text style={styles.hint}>
                Score 0 = negative, 1 = positive.
              </Text>
            </View>
          ) : null}
        </>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  root: {
    flex: 1,
    padding: 16,
    backgroundColor: "#0f172a",
    gap: 12,
  },
  header: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  title: {
    color: "#f8fafc",
    fontSize: 15,
    fontWeight: "600",
  },
  badge: {
    paddingHorizontal: 8,
    paddingVertical: 3,
    borderRadius: 999,
    backgroundColor: "#334155",
  },
  badgeWebgpu: {
    backgroundColor: "#312e81",
  },
  badgeText: {
    color: "#e2e8f0",
    fontSize: 11,
    fontWeight: "600",
    textTransform: "uppercase",
    letterSpacing: 0.5,
  },
  center: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    gap: 10,
  },
  statusText: {
    color: "#cbd5e1",
    fontSize: 13,
  },
  progressTrack: {
    width: "80%",
    height: 4,
    borderRadius: 2,
    backgroundColor: "#1e293b",
    overflow: "hidden",
  },
  progressFill: {
    height: "100%",
    backgroundColor: "#6366f1",
  },
  errorText: {
    color: "#fca5a5",
    fontSize: 13,
    textAlign: "center",
    paddingHorizontal: 12,
  },
  input: {
    minHeight: 72,
    borderWidth: 1,
    borderColor: "#334155",
    borderRadius: 10,
    padding: 12,
    color: "#f1f5f9",
    fontSize: 14,
    backgroundColor: "#1e293b",
  },
  button: {
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#6366f1",
    borderRadius: 10,
    paddingVertical: 10,
  },
  buttonDisabled: {
    opacity: 0.7,
  },
  buttonText: {
    color: "#fff",
    fontSize: 14,
    fontWeight: "600",
  },
  result: {
    gap: 8,
    marginTop: 4,
  },
  resultRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  resultLabel: {
    color: "#f8fafc",
    fontSize: 16,
    fontWeight: "600",
  },
  scoreText: {
    color: "#94a3b8",
    fontSize: 13,
    fontFamily: "monospace",
  },
  barTrack: {
    height: 8,
    borderRadius: 4,
    backgroundColor: "#1e293b",
    overflow: "hidden",
  },
  barFill: {
    height: "100%",
    borderRadius: 4,
  },
  hint: {
    color: "#64748b",
    fontSize: 11,
    lineHeight: 16,
  },
  hintMono: {
    fontFamily: "monospace",
    color: "#94a3b8",
  },
});
