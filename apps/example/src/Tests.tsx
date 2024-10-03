/* eslint-disable no-eval */
import React, { useEffect, useState } from "react";
import { Dimensions, Text, View, Image } from "react-native";
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
console.log({
  CI,
  presentationFormat,
});

const useWebGPU = () => {
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      console.log("Requesting adapter");
      const a = await navigator.gpu.requestAdapter();
      if (!a) {
        throw new Error("No adapter");
      }
      console.log("Requesting device");
      const d = await a.requestDevice();
      setAdapter(a);
      setDevice(d);
      console.log("Ready");
    })();
  }, []);
  return { adapter, device };
};

export const Tests = ({ assets: { di3D, saturn, moon } }: AssetProps) => {
  const [texture, setTexture] = useState<GPUTexture | null>(null);
  const { adapter, device } = useWebGPU();
  const [client, hostname] = useClient();
  useEffect(() => {
    if (client !== null && adapter !== null && device !== null) {
      console.log("p1");
      client.onmessage = (e) => {
        console.log("GOT A MESSAGE");
        const tree = JSON.parse(e.data);
        if (tree.code) {
          const canvas = new GPUOffscreenCanvas(1024, 1024);
          const ctx = canvas.getContext("webgpu")!;
          ctx.configure({
            device,
            format: presentationFormat,
            alphaMode: "premultiplied",
          });
          const result = eval(
            `(function Main() {
              return (${tree.code})(this.ctx);
            })`
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
