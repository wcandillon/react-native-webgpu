/* eslint-disable no-eval */

import React, { useEffect } from "react";
import { Text, View, Image } from "react-native";
import { Canvas, useDevice, useGPUContext } from "react-native-wgpu";
import { mat4, vec3, mat3 } from "wgpu-matrix";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./Triangle/triangle";
import type { AssetProps } from "./components/useAssets";

export const CI = process.env.CI === "true";

const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

export const Tests = ({ assets: { di3D, saturn, moon } }: AssetProps) => {
  const { adapter, device } = useDevice();
  const { ref, context } = useGPUContext();
  const [client, hostname] = useClient();
  useEffect(() => {
    if (
      client !== null &&
      adapter !== null &&
      device !== null &&
      context !== null
    ) {
      client.onmessage = (e) => {
        const tree = JSON.parse(e.data);
        if (tree.code) {
          context.configure({
            device,
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
              ctx: context,
              canvas: context.canvas,
              mat4,
              vec3,
              mat3,
              ...tree.ctx,
            },
          });
          context.present();
          if (result instanceof Promise) {
            result.then((r) => {
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
  }, [adapter, client, context, device, di3D, moon, saturn]);
  return (
    <View style={{ flex: 1, backgroundColor: "white" }}>
      <Text style={{ color: "black" }}>
        {client === null
          ? `⚪️ Connecting to ${hostname}. Use yarn e2e to run tests.`
          : "🟢 Waiting for the server to send tests"}
      </Text>
      <Canvas ref={ref} style={{ width: 1024, height: 1024 }} />
    </View>
  );
};
