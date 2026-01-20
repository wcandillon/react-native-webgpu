import React, { useEffect, useState, useCallback, useRef } from "react";
import {
  View,
  Text,
  FlatList,
  TouchableOpacity,
  StyleSheet,
  TextInput,
  ActivityIndicator,
  ScrollView,
} from "react-native";

import { CTSRunner, TestResult, TestRunSummary } from "../webgpu-cts/src/common/runtime/rn";
import { allSpecs } from "../webgpu-cts/src/common/runtime/rn/generated/all_specs";

type TestStatus = "idle" | "loading" | "ready" | "running" | "complete" | "error";

interface TestItemProps {
  result: TestResult;
}

const TestItem = ({ result }: TestItemProps) => {
  const [expanded, setExpanded] = useState(false);

  const statusColor = {
    pass: "#4CAF50",
    fail: "#F44336",
    skip: "#9E9E9E",
    warn: "#FF9800",
    running: "#2196F3",
    notrun: "#9E9E9E",
  }[result.status];

  return (
    <TouchableOpacity
      style={styles.testItem}
      onPress={() => setExpanded(!expanded)}
    >
      <View style={styles.testHeader}>
        <View style={[styles.statusDot, { backgroundColor: statusColor }]} />
        <Text style={styles.testName} numberOfLines={expanded ? undefined : 1}>
          {result.name}
        </Text>
        <Text style={styles.testTime}>{result.timems.toFixed(1)}ms</Text>
      </View>
      {expanded && result.logs && result.logs.length > 0 && (
        <View style={styles.logsContainer}>
          {result.logs.map((log, i) => (
            <Text key={i} style={styles.logText}>
              {log}
            </Text>
          ))}
        </View>
      )}
    </TouchableOpacity>
  );
};

// Preset queries for quick testing
const PRESET_QUERIES = [
  { label: "Adapter Info", query: "webgpu:api,operation,adapter,info:*" },
  { label: "Request Adapter", query: "webgpu:api,operation,adapter,requestAdapter:*" },
  { label: "All Adapter", query: "webgpu:api,operation,adapter,*" },
  { label: "Labels", query: "webgpu:api,operation,labels:*" },
];

export const CTS = () => {
  // Start with a very simple query - just adapter info tests
  const [query, setQuery] = useState("webgpu:api,operation,adapter,info:*");
  const [status, setStatus] = useState<TestStatus>("loading");
  const [error, setError] = useState<string | null>(null);
  const [results, setResults] = useState<TestResult[]>([]);
  const [summary, setSummary] = useState<TestRunSummary | null>(null);
  const [progress, setProgress] = useState({ current: 0, total: 0, name: "" });
  const [specCount, setSpecCount] = useState(0);

  const runnerRef = useRef<CTSRunner | null>(null);

  useEffect(() => {
    // Initialize runner
    try {
      console.log("[CTS] Initializing runner...");
      console.log("[CTS] allSpecs loaded:", allSpecs ? "yes" : "no");
      if (allSpecs) {
        const webgpuSpecs = allSpecs.get("webgpu");
        console.log("[CTS] webgpu specs count:", webgpuSpecs?.length ?? 0);
        setSpecCount(webgpuSpecs?.length ?? 0);
      }

      runnerRef.current = new CTSRunner(allSpecs, {
        debug: true,
      });
      console.log("[CTS] Runner initialized successfully");
      setStatus("ready");
    } catch (e) {
      console.error("[CTS] Failed to initialize runner:", e);
      setError(e instanceof Error ? e.message : String(e));
      setStatus("error");
    }

    return () => {
      runnerRef.current?.requestStop();
    };
  }, []);

  const runTests = useCallback(async () => {
    if (!runnerRef.current || status === "running") return;

    setStatus("running");
    setResults([]);
    setSummary(null);
    setError(null);
    setProgress({ current: 0, total: 0, name: "" });

    try {
      console.log("[CTS] Starting test run with query:", query);
      const { summary: runSummary, results: runResults } =
        await runnerRef.current.runTests(query, {
          onTestStart: (name, index, total) => {
            console.log(`[CTS] Running ${index + 1}/${total}: ${name}`);
            setProgress({ current: index + 1, total, name });
          },
          onTestComplete: (result) => {
            console.log(`[CTS] ${result.status}: ${result.name}`);
            setResults((prev) => [...prev, result]);
          },
          shouldStop: () => runnerRef.current?.isStopRequested() ?? false,
        });

      console.log("[CTS] Test run complete:", runSummary);
      setSummary(runSummary);
      setStatus("complete");
    } catch (e) {
      console.error("[CTS] Test run failed:", e);
      setError(e instanceof Error ? e.message : String(e));
      setStatus("error");
    }
  }, [query, status]);

  const stopTests = useCallback(() => {
    runnerRef.current?.requestStop();
  }, []);

  // Show loading state
  if (status === "loading") {
    return (
      <View style={[styles.container, styles.centered]}>
        <ActivityIndicator size="large" color="#2196F3" />
        <Text style={styles.loadingText}>Loading CTS...</Text>
      </View>
    );
  }

  // Show error state
  if (status === "error" && !results.length) {
    return (
      <View style={[styles.container, styles.centered]}>
        <Text style={styles.errorTitle}>Error</Text>
        <ScrollView style={styles.errorScroll}>
          <Text style={styles.errorText}>{error}</Text>
        </ScrollView>
        <TouchableOpacity
          style={styles.retryButton}
          onPress={() => {
            setError(null);
            setStatus("ready");
          }}
        >
          <Text style={styles.buttonText}>Retry</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.title}>WebGPU CTS</Text>
        <Text style={styles.subtitle}>{specCount} spec files loaded</Text>

        {/* Preset query buttons */}
        <ScrollView horizontal style={styles.presetRow} showsHorizontalScrollIndicator={false}>
          {PRESET_QUERIES.map((preset) => (
            <TouchableOpacity
              key={preset.query}
              style={[
                styles.presetButton,
                query === preset.query && styles.presetButtonActive,
              ]}
              onPress={() => setQuery(preset.query)}
              disabled={status === "running"}
            >
              <Text
                style={[
                  styles.presetButtonText,
                  query === preset.query && styles.presetButtonTextActive,
                ]}
              >
                {preset.label}
              </Text>
            </TouchableOpacity>
          ))}
        </ScrollView>

        <TextInput
          style={styles.queryInput}
          value={query}
          onChangeText={setQuery}
          placeholder="Enter test query..."
          placeholderTextColor="#999"
          editable={status !== "running"}
        />
        <View style={styles.buttonRow}>
          {status === "running" ? (
            <TouchableOpacity style={styles.stopButton} onPress={stopTests}>
              <Text style={styles.buttonText}>Stop</Text>
            </TouchableOpacity>
          ) : (
            <TouchableOpacity style={styles.runButton} onPress={runTests}>
              <Text style={styles.buttonText}>Run Tests</Text>
            </TouchableOpacity>
          )}
        </View>

        {error && (
          <View style={styles.errorBanner}>
            <Text style={styles.errorBannerText}>{error}</Text>
          </View>
        )}
      </View>

      {status === "running" && (
        <View style={styles.progressContainer}>
          <ActivityIndicator size="small" color="#2196F3" />
          <Text style={styles.progressText}>
            Running {progress.current}/{progress.total}
          </Text>
          <Text style={styles.progressName} numberOfLines={1}>
            {progress.name}
          </Text>
        </View>
      )}

      {summary && (
        <View style={styles.summaryContainer}>
          <Text style={styles.summaryTitle}>Summary</Text>
          <View style={styles.summaryRow}>
            <Text style={[styles.summaryItem, { color: "#4CAF50" }]}>
              Pass: {summary.passed}
            </Text>
            <Text style={[styles.summaryItem, { color: "#F44336" }]}>
              Fail: {summary.failed}
            </Text>
            <Text style={[styles.summaryItem, { color: "#9E9E9E" }]}>
              Skip: {summary.skipped}
            </Text>
            <Text style={[styles.summaryItem, { color: "#FF9800" }]}>
              Warn: {summary.warned}
            </Text>
          </View>
          <Text style={styles.summaryTime}>
            Total: {summary.total} tests in {(summary.timems / 1000).toFixed(2)}s
          </Text>
        </View>
      )}

      <FlatList
        data={results}
        keyExtractor={(item) => item.name}
        renderItem={({ item }) => <TestItem result={item} />}
        style={styles.list}
        contentContainerStyle={styles.listContent}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "#1a1a1a",
  },
  centered: {
    justifyContent: "center",
    alignItems: "center",
    padding: 20,
  },
  loadingText: {
    color: "#fff",
    fontSize: 16,
    marginTop: 16,
  },
  errorTitle: {
    color: "#F44336",
    fontSize: 24,
    fontWeight: "bold",
    marginBottom: 16,
  },
  errorScroll: {
    maxHeight: 200,
    marginBottom: 16,
  },
  errorText: {
    color: "#F44336",
    fontSize: 14,
    fontFamily: "monospace",
  },
  retryButton: {
    backgroundColor: "#2196F3",
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
  },
  errorBanner: {
    backgroundColor: "#F44336",
    padding: 12,
    borderRadius: 8,
    marginTop: 12,
  },
  errorBannerText: {
    color: "#fff",
    fontSize: 12,
  },
  header: {
    padding: 16,
    borderBottomWidth: 1,
    borderBottomColor: "#333",
  },
  title: {
    fontSize: 24,
    fontWeight: "bold",
    color: "#fff",
    marginBottom: 4,
  },
  subtitle: {
    fontSize: 12,
    color: "#999",
    marginBottom: 12,
  },
  presetRow: {
    marginBottom: 12,
  },
  presetButton: {
    backgroundColor: "#2a2a2a",
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 16,
    marginRight: 8,
    borderWidth: 1,
    borderColor: "#444",
  },
  presetButtonActive: {
    backgroundColor: "#2196F3",
    borderColor: "#2196F3",
  },
  presetButtonText: {
    color: "#999",
    fontSize: 12,
  },
  presetButtonTextActive: {
    color: "#fff",
  },
  queryInput: {
    backgroundColor: "#2a2a2a",
    borderRadius: 8,
    padding: 12,
    color: "#fff",
    fontSize: 14,
    fontFamily: "monospace",
    marginBottom: 12,
  },
  buttonRow: {
    flexDirection: "row",
    gap: 8,
  },
  runButton: {
    backgroundColor: "#4CAF50",
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
  },
  stopButton: {
    backgroundColor: "#F44336",
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
  },
  buttonText: {
    color: "#fff",
    fontWeight: "bold",
    fontSize: 16,
  },
  progressContainer: {
    flexDirection: "row",
    alignItems: "center",
    padding: 12,
    backgroundColor: "#2a2a2a",
    gap: 8,
  },
  progressText: {
    color: "#fff",
    fontSize: 14,
  },
  progressName: {
    color: "#999",
    fontSize: 12,
    flex: 1,
  },
  summaryContainer: {
    padding: 16,
    backgroundColor: "#2a2a2a",
    margin: 8,
    borderRadius: 8,
  },
  summaryTitle: {
    color: "#fff",
    fontSize: 18,
    fontWeight: "bold",
    marginBottom: 8,
  },
  summaryRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    marginBottom: 8,
  },
  summaryItem: {
    fontSize: 14,
    fontWeight: "500",
  },
  summaryTime: {
    color: "#999",
    fontSize: 12,
  },
  list: {
    flex: 1,
  },
  listContent: {
    padding: 8,
  },
  testItem: {
    backgroundColor: "#2a2a2a",
    borderRadius: 8,
    padding: 12,
    marginBottom: 4,
  },
  testHeader: {
    flexDirection: "row",
    alignItems: "center",
  },
  statusDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    marginRight: 8,
  },
  testName: {
    flex: 1,
    color: "#fff",
    fontSize: 12,
    fontFamily: "monospace",
  },
  testTime: {
    color: "#999",
    fontSize: 11,
    marginLeft: 8,
  },
  logsContainer: {
    marginTop: 8,
    padding: 8,
    backgroundColor: "#1a1a1a",
    borderRadius: 4,
  },
  logText: {
    color: "#F44336",
    fontSize: 11,
    fontFamily: "monospace",
  },
});
