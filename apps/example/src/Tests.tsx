import React, { useEffect, useState } from "react";
import { Dimensions, Text, View, Image } from "react-native";
import { GPUOffscreenCanvas } from "react-native-webgpu";
import { mat4, vec3, mat3 } from "wgpu-matrix";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./Triangle/triangle";
import type { AssetProps } from "./components/useAssets";
import { Texture } from "./components/Texture";

export const CI = process.env.CI === "true";

const { width } = Dimensions.get("window");
const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

export const Tests = ({ assets: { di3D, saturn, moon } }: AssetProps) => {
  const [texture, setTexture] = useState<GPUTexture | null>(null);
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  const [setupError, setSetupError] = useState<string | null>(null);
  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const a = await navigator.gpu.requestAdapter();
        if (!a) {
          throw new Error("No appropriate GPUAdapter found.");
        }
        // "rnwebgpu/native-texture" is enabled by default whenever the adapter
        // supports it, so the shared-texture / importExternalTexture specs get
        // the capability without requesting it here. Specs that need it still
        // gate on device.features.has(...) and skip where it is unavailable.
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
      <Texture
        texture={texture}
        device={device}
        style={{ width: width, height: width }}
      />
    </View>
  );
};
