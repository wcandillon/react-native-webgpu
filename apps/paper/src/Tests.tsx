/* eslint-disable no-eval */

import React, { useEffect, useState } from "react";
import { Dimensions, Platform, Text, View, Image } from "react-native";
import "react-native-wgpu";
import { mat4, vec3, mat3 } from "wgpu-matrix";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./Triangle/triangle";
import { NativeDrawingContext } from "./components/NativeDrawingContext";
import type { AssetProps } from "./components/useAssets";
import { Bitmap } from "./components/Bitmap";

export const CI = process.env.CI === "true";

const { width } = Dimensions.get("window");

const useWebGPU = () => {
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      const a = await navigator.gpu.requestAdapter();
      if (!a) {
        throw new Error("No adapter");
      }
      const d = await a.requestDevice();
      setAdapter(a);
      setDevice(d);
    })();
  }, []);
  return { adapter, device };
};

export const Tests = ({ assets: { di3D, saturn, moon } }: AssetProps) => {
  const [bitmap, setBitmap] = useState<Bitmap | null>(null);
  const { adapter, device } = useWebGPU();
  const [client, hostname] = useClient();
  useEffect(() => {
    if (client !== null && adapter !== null && device !== null) {
      client.onmessage = (e) => {
        const tree = JSON.parse(e.data);
        if (tree.code) {
          const ctx = new NativeDrawingContext(device, 1024, 1024);
          const result = eval(
            `(function Main() {
              return (${tree.code})(this.ctx);
            })`,
          ).call({
            ctx: {
              gpu: navigator.gpu,
              adapter,
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
              mat4,
              vec3,
              mat3,
              ...tree.ctx,
            },
          });
          if (result instanceof Promise) {
            result.then((r) => {
              if (r.data && r.width && r.height) {
                const img = {
                  width: r.width,
                  height: r.height,
                  colorType:
                    Platform.OS === "ios"
                      ? ("bgra8unorm" as const)
                      : ("rgba8unorm" as const),
                  data: new Uint8Array(r.data),
                };
                setBitmap(img);
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
  return (
    <View style={{ flex: 1, backgroundColor: "white" }}>
      <Text style={{ color: "black" }}>
        {client === null
          ? `⚪️ Connecting to ${hostname}. Use yarn e2e to run tests.`
          : "🟢 Waiting for the server to send tests"}
      </Text>
      <Bitmap bitmap={bitmap} style={{ width: width, height: width }} />
    </View>
  );
};
