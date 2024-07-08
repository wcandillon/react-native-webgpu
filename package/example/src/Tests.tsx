/* eslint-disable no-eval */
/* eslint-disable @typescript-eslint/no-explicit-any */
import React, { useEffect } from "react";
import { Text, View } from "react-native";
import { gpu } from "react-native-webgpu";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";

export const CI = process.env.CI === "true";

const useWebGPU = () => {
  const [adapter, setAdapter] = React.useState<GPUAdapter | null>(null);
  const [device, setDevice] = React.useState<GPUDevice | null>(null);
  useEffect(() => {
    (async () => {
      const a = await gpu.requestAdapter();
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

export const Tests = () => {
  const { adapter, device } = useWebGPU();
  const [client, hostname] = useClient();
  useEffect(() => {
    if (client !== null && adapter !== null && device !== null) {
      client.onmessage = (e) => {
        const tree: any = JSON.parse(e.data);
        if (tree.code) {
          const result = eval(
            `(function Main() {
              return (${tree.code})(this.ctx);
            })`,
          ).call({
            ctx: {
              ...tree.ctx,
              gpu,
              adapter,
              device,
              GPUBufferUsage,
              GPUColorWrite,
              GPUMapMode,
              GPUShaderStage,
              GPUTextureUsage,
              cubeVertexArray,
            },
          });
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
  }, [adapter, client, device]);
  return (
    <View style={{ flex: 1, backgroundColor: "white" }}>
      <Text style={{ color: "black" }}>
        {client === null
          ? `⚪️ Connecting to ${hostname}. Use yarn e2e to run tests.`
          : "🟢 Waiting for the server to send tests"}
      </Text>
    </View>
  );
};
