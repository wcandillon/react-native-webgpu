/* eslint-disable no-eval */

import React, { useEffect, useState } from "react";
import { Dimensions, Text, View, Image } from "react-native";
import { GPUOffscreenCanvas, useDevice } from "react-native-wgpu";
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
  const { adapter, device } = useDevice();
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
  if (!device) {
    return null;
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
