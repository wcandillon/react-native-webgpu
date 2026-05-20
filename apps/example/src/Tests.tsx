import React, { useEffect, useState } from "react";
import { Dimensions, Platform, Text, View, Image } from "react-native";
import { GPUOffscreenCanvas } from "react-native-wgpu";
import { mat4, vec3, mat3 } from "wgpu-matrix";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./Triangle/triangle";
import type { AssetProps } from "./components/useAssets";
import { Texture } from "./components/Texture";

export const CI = process.env.CI === "true";

const { width } = Dimensions.get("window");
const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

// Platform-specific Dawn features that back importSharedTextureMemory. The
// fence feature is paired with the memory feature because Dawn validates it
// is enabled before letting BeginAccess/EndAccess succeed on the matching
// backend, even when callers pass zero fences.
const sharedTextureMemoryFeatures = (): GPUFeatureName[] => {
  if (Platform.OS === "ios") {
    return [
      "shared-texture-memory-iosurface",
      "shared-fence-mtl-shared-event",
    ] as unknown as GPUFeatureName[];
  }
  if (Platform.OS === "android") {
    return [
      "shared-texture-memory-ahardware-buffer",
      "shared-fence-vk-semaphore-sync-fd",
    ] as unknown as GPUFeatureName[];
  }
  return [];
};
const OPTIONAL_SHARED_TEXTURE_MEMORY_FEATURES = sharedTextureMemoryFeatures();

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
        const requiredFeatures = OPTIONAL_SHARED_TEXTURE_MEMORY_FEATURES.filter(
          (f) => a.features.has(f),
        );
        console.log(
          `[Tests] adapter features (shared-*): ${[...a.features]
            .filter((f) => f.toString().startsWith("shared-"))
            .join(", ")}`,
        );
        console.log(
          `[Tests] requestDevice with requiredFeatures: ${JSON.stringify(requiredFeatures)}`,
        );
        const d = await a.requestDevice({ requiredFeatures });
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
          const result = eval(
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
          if (result instanceof Promise) {
            result.then((r) => {
              if (r.data && r.width && r.height) {
                setTexture(ctx.getCurrentTexture());
              }
              client.send(JSON.stringify(r));
            });
          } else {
            client.send(JSON.stringify(result));
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
