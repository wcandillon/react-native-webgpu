import { useCallback, useState } from "react";
import { StyleSheet } from "react-native";
import type { RNCanvasContext } from "react-native-webgpu";

// Shared plumbing for the surface lifecycle repros. Each screen renders a
// trivial animated clear so that getCurrentTexture()/submit()/present() run
// every frame; validation errors are surfaced through the uncapturederror
// event so lifecycle holes that do not hard-crash still show up in the log.

export interface DiagnosticGPU {
  device: GPUDevice;
  format: GPUTextureFormat;
}

export const initGPU = async (
  onLog: (message: string) => void,
): Promise<DiagnosticGPU> => {
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    throw new Error("No adapter");
  }
  const device = await adapter.requestDevice();
  device.addEventListener("uncapturederror", (event) => {
    const { error } = event as GPUUncapturedErrorEvent;
    onLog(`[uncaptured] ${error.message.split("\n")[0]}`);
  });
  device.lost.then((info) => {
    onLog(`[device lost] ${info.reason}: ${info.message}`);
  });
  return { device, format: navigator.gpu.getPreferredCanvasFormat() };
};

export const drawClearFrame = (
  device: GPUDevice,
  context: RNCanvasContext,
  frame: number,
) => {
  const texture = context.getCurrentTexture();
  const view = texture.createView();
  const encoder = device.createCommandEncoder();
  const t = frame / 60;
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        view,
        clearValue: [
          0.5 + 0.5 * Math.sin(t),
          0.5 + 0.5 * Math.sin(t + 2.1),
          0.5 + 0.5 * Math.sin(t + 4.2),
          1,
        ],
        loadOp: "clear",
        storeOp: "store",
      },
    ],
  });
  pass.end();
  device.queue.submit([encoder.finish()]);
  context.present();
};

export const useDiagnosticLog = () => {
  const [log, setLog] = useState<string[]>([]);
  const append = useCallback((line: string) => {
    console.log(`[surface-lifecycle] ${line}`);
    setLog((prev) => [...prev.slice(-24), line]);
  }, []);
  return { log, append };
};

export const diagnosticStyles = StyleSheet.create({
  container: {
    flex: 1,
  },
  controls: {
    padding: 12,
    gap: 8,
  },
  description: {
    fontSize: 12,
    color: "#333",
  },
  canvas: {
    flex: 1,
  },
  log: {
    maxHeight: 200,
    padding: 12,
    backgroundColor: "#111",
  },
  logLine: {
    fontFamily: "Menlo",
    fontSize: 10,
    color: "#0f0",
  },
});
