import React, { useEffect, useRef, useState } from "react";
import {
  Platform,
  PixelRatio,
  StyleSheet,
  Switch,
  Text,
  View,
} from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";

import { fullscreenTriangleVertWGSL, hdrBandFragWGSL } from "./shaders";

type ToneMapping = "standard" | "extended";

const HDR_FORMAT: GPUTextureFormat = "rgba16float";
const PEAK_MULTIPLIER = 8.0; // 8x SDR reference white.

function HDRCanvas({
  toneMapping,
  peak,
}: {
  toneMapping: ToneMapping;
  peak: number;
}) {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No adapter");
      }
      const device = await adapter.requestDevice();
      if (cancelled) {
        return;
      }

      const context = ref.current!.getContext("webgpu")!;
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      context.configure({
        device,
        format: HDR_FORMAT,
        alphaMode: "opaque",
        toneMapping: { mode: toneMapping },
      });

      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: device.createShaderModule({
            code: fullscreenTriangleVertWGSL,
          }),
          entryPoint: "main",
        },
        fragment: {
          module: device.createShaderModule({ code: hdrBandFragWGSL }),
          entryPoint: "main",
          targets: [{ format: HDR_FORMAT }],
        },
        primitive: { topology: "triangle-list" },
      });

      const paramsBuffer = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(
        paramsBuffer,
        0,
        new Float32Array([peak, 0, 0, 0]),
      );

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{ binding: 0, resource: { buffer: paramsBuffer } }],
      });

      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: context.getCurrentTexture().createView(),
            clearValue: [0, 0, 0, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
    })();
    return () => {
      cancelled = true;
    };
  }, [toneMapping, peak]);

  return <Canvas ref={ref} style={StyleSheet.absoluteFill} />;
}

export function HDR() {
  const [extended, setExtended] = useState(true);
  const mode: ToneMapping = extended ? "extended" : "standard";
  return (
    <View style={styles.container}>
      <View style={styles.toolbar}>
        <Text style={styles.title}>HDR — {mode}</Text>
        <View style={styles.switchRow}>
          <Text style={styles.switchLabel}>Extended</Text>
          <Switch value={extended} onValueChange={setExtended} />
        </View>
      </View>

      <Text style={styles.hint}>
        Left band: SDR white (1.0). Right band: {PEAK_MULTIPLIER}x. Toggle the
        switch. With "extended" on an EDR display (iPhone Pro / iPad Pro XDR /
        MBP XDR), the right band glows visibly brighter than the left. In
        "standard" both bands match.
      </Text>
      <Text style={styles.hint}>
        Tip: dim the display brightness; the OS allocates more EDR headroom at
        lower SDR brightness, so the boost is more obvious. iOS Settings,
        Display & Brightness, Auto-Brightness can also affect headroom.
      </Text>

      <View style={styles.canvasContainer}>
        {/* Force a fresh CAMetalLayer per mode: iOS won't downgrade a layer
            out of EDR composition once it's been promoted, so we remount the
            Canvas (and therefore the underlying MetalView) when the toggle
            flips. */}
        <HDRCanvas key={mode} toneMapping={mode} peak={PEAK_MULTIPLIER} />
      </View>

      {Platform.OS !== "ios" && Platform.OS !== "macos" ? (
        <Text style={styles.warning}>
          Note: HDR display is currently only wired for Apple platforms.
        </Text>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "black",
  },
  toolbar: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    padding: 12,
  },
  title: {
    color: "white",
    fontSize: 14,
    fontWeight: "600",
    flex: 1,
  },
  switchRow: {
    flexDirection: "row",
    alignItems: "center",
  },
  switchLabel: {
    color: "white",
    marginRight: 8,
  },
  hint: {
    color: "#bbb",
    fontSize: 12,
    paddingHorizontal: 12,
    paddingBottom: 8,
  },
  canvasContainer: {
    flex: 1,
    margin: 12,
    backgroundColor: "black",
  },
  warning: {
    color: "#ffb347",
    fontSize: 12,
    padding: 12,
  },
});
