import React, { useEffect, useRef, useState } from "react";
import {
  Dimensions,
  Text,
  View,
  Image,
  PixelRatio,
  findNodeHandle,
} from "react-native";
import { Canvas, GPUOffscreenCanvas } from "react-native-webgpu";
import type { CanvasRef } from "react-native-webgpu";
import { mat4, vec3, mat3 } from "wgpu-matrix";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./Triangle/triangle";
import type { AssetProps } from "./components/useAssets";
import { Texture } from "./components/Texture";
import { workletDeviceStress } from "./components/workletDeviceStress";
import { workletSerialization } from "./components/workletSerialization";

export const CI = process.env.CI === "true";

const { width } = Dimensions.get("window");
const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
const PROBE_SIZE = 64;
const QUADRANT = PROBE_SIZE / 2;
const SCALED_CANVAS_SIZE = { width: 64, height: 32 };

export const Tests = ({ assets: { di3D, saturn, moon } }: AssetProps) => {
  const [texture, setTexture] = useState<GPUTexture | null>(null);
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  const [setupError, setSetupError] = useState<string | null>(null);
  // A fixed native view the drawElementImageToTexture spec snapshots: four
  // 32x32pt solid quadrants (red, lime, blue, white), 64x64pt in total.
  const probeRef = useRef<View>(null);
  // The Canvas spec measures this canvas, drawn at half its layout size, and
  // re-renders the screen to check that it keeps its ref handle.
  const scaledCanvasRef = useRef<CanvasRef>(null);
  const [renderRequests, setRenderRequests] = useState(0);
  const renderCommitted = useRef<(() => void) | null>(null);
  useEffect(() => {
    renderCommitted.current?.();
    renderCommitted.current = null;
  }, [renderRequests]);
  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const a = await navigator.gpu.requestAdapter();
        if (!a) {
          throw new Error("No appropriate GPUAdapter found.");
        }
        // Native frame import is enabled by default whenever the adapter
        // supports it, so the shared-texture / importExternalTexture specs get
        // the capability without requesting it here; they skip where the
        // adapter has no shared texture memory.
        const d = await a.requestDevice();
        if (!d) {
          throw new Error("No appropriate GPUDevice found.");
        }
        if (cancelled) {
          return;
        }
        setAdapter(a);
        setDevice(d);
      } catch (e) {
        const msg = e instanceof Error ? e.message : String(e);
        console.warn(`[Tests] device setup failed: ${msg}`);
        if (!cancelled) {
          setSetupError(msg);
        }
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);
  const [client, hostname] = useClient();
  // Without a device, answer every eval with the setup error right away. The
  // host would otherwise wait out its 30 s eval timeout on every single test,
  // turning one failure into an hour-long run.
  useEffect(() => {
    if (client !== null && setupError !== null) {
      client.onmessage = () => {
        client.send(
          JSON.stringify({ $$error: `device setup failed: ${setupError}` }),
        );
      };
    }
  }, [client, setupError]);
  useEffect(() => {
    if (client !== null && adapter !== null && device !== null) {
      client.onmessage = (e) => {
        const tree = JSON.parse(e.data);
        if (tree.code) {
          const canvas = new GPUOffscreenCanvas(1024, 1024);
          const ctx = canvas.getContext("webgpu")!;
          ctx.configure({
            device: device!,
            format: presentationFormat,
            alphaMode: "premultiplied",
          });
          const runEval = () =>
            eval(
              `(function Main() {
              return (${tree.code})(this.ctx);
            })`,
            ).call({
              ctx: {
                gpu: navigator.gpu,
                device,
                urls: {
                  fTexture: Image.resolveAssetSource(require("./assets/f.png"))
                    .uri,
                },
                assets: {
                  cubeVertexArray,
                  di3D,
                  saturn,
                  moon,
                },
                shaders: {
                  triangleVertWGSL,
                  redFragWGSL,
                },
                ctx,
                canvas: ctx.canvas,
                views: {
                  probe: findNodeHandle(probeRef.current),
                  probeSize: PROBE_SIZE,
                  pixelRatio: PixelRatio.get(),
                  scaledCanvas: () => scaledCanvasRef.current,
                  scaledCanvasSize: SCALED_CANVAS_SIZE,
                  rerender: () =>
                    new Promise<void>((resolve) => {
                      renderCommitted.current = resolve;
                      setRenderRequests((requests) => requests + 1);
                    }),
                },
                workletDeviceStress,
                workletSerialization,
                mat4,
                vec3,
                mat3,
                ...tree.ctx,
              },
            });
          const onError = (error: unknown) => {
            // Report a thrown error back to the host so the matching test can
            // `.rejects` instead of hanging, and so the throw is not surfaced
            // as an unhandled rejection (which logs console.error and could
            // break the socket for subsequent tests).
            const message =
              error instanceof Error ? error.message : String(error);
            client.send(JSON.stringify({ $$error: message }));
          };
          try {
            const result = runEval();
            if (result instanceof Promise) {
              result
                .then((r) => {
                  if (r.data && r.width && r.height) {
                    setTexture(ctx.getCurrentTexture());
                  }
                  client.send(JSON.stringify(r));
                })
                .catch(onError);
            } else {
              client.send(JSON.stringify(result));
            }
          } catch (error) {
            onError(error);
          }
        }
      };
      return () => {
        client.close();
      };
    }
    return;
  }, [adapter, client, device, di3D, moon, saturn]);
  if (setupError) {
    return (
      <View style={{ flex: 1, padding: 16, backgroundColor: "white" }}>
        <Text style={{ color: "red" }}>
          🔴 Test device setup failed: {setupError}
        </Text>
      </View>
    );
  }
  if (!device) {
    return (
      <View style={{ flex: 1, padding: 16, backgroundColor: "white" }}>
        <Text style={{ color: "black" }}>⏳ Initializing GPU device…</Text>
      </View>
    );
  }
  return (
    <View style={{ flex: 1, backgroundColor: "white" }}>
      <Text style={{ color: "black" }}>
        {client === null
          ? `⚪️ Connecting to ${hostname}. Use yarn e2e to run tests.`
          : "🟢 Waiting for the server to send tests"}
      </Text>
      <View
        ref={probeRef}
        collapsable={false}
        style={{
          width: PROBE_SIZE,
          height: PROBE_SIZE,
          flexDirection: "row",
          flexWrap: "wrap",
        }}
      >
        <View
          style={{ width: QUADRANT, height: QUADRANT, backgroundColor: "red" }}
        />
        <View
          style={{ width: QUADRANT, height: QUADRANT, backgroundColor: "lime" }}
        />
        <View
          style={{ width: QUADRANT, height: QUADRANT, backgroundColor: "blue" }}
        />
        <View
          style={{
            width: QUADRANT,
            height: QUADRANT,
            backgroundColor: "white",
          }}
        />
      </View>
      <Canvas
        ref={scaledCanvasRef}
        style={{ ...SCALED_CANVAS_SIZE, transform: [{ scale: 0.5 }] }}
      />
      <Texture
        texture={texture}
        device={device}
        style={{ width: width, height: width }}
      />
    </View>
  );
};
