/* eslint-disable no-eval */

import React, { useEffect, useState } from "react";
import { Dimensions, Platform, Text, View } from "react-native";
import { gpu } from "react-native-webgpu";
import { mat4, vec3, mat3 } from "wgpu-matrix";
import type { SkImage } from "@shopify/react-native-skia";
import {
  AlphaType,
  Canvas,
  ColorType,
  Image,
  Skia,
} from "@shopify/react-native-skia";

import { useClient } from "./useClient";
import { cubeVertexArray } from "./components/cube";
import { redFragWGSL, triangleVertWGSL } from "./components/triangle";
import { NativeDrawingContext } from "./components/NativeDrawingContext";

export const CI = process.env.CI === "true";

const { width } = Dimensions.get("window");

const useWebGPU = () => {
  const [adapter, setAdapter] = useState<GPUAdapter | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);
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

interface TestsProps {
  assets: { di3D: ImageData; saturn: ImageData; moon: ImageData };
}

export const Tests = ({ assets: { di3D, saturn, moon } }: TestsProps) => {
  const [image, setImage] = useState<SkImage | null>(null);
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
              gpu,
              adapter,
              device,
              GPUBufferUsage,
              GPUColorWrite,
              GPUMapMode,
              GPUShaderStage,
              GPUTextureUsage,
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
                const data = Skia.Data.fromBytes(new Uint8Array(r.data));
                const img = Skia.Image.MakeImage(
                  {
                    width: r.width,
                    height: r.height,
                    alphaType: AlphaType.Premul,
                    colorType:
                      Platform.OS === "ios"
                        ? ColorType.BGRA_8888
                        : ColorType.RGBA_8888,
                  },
                  data,
                  4 * r.width,
                );
                setImage(img);
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
      <Canvas style={{ width, height: width }}>
        <Image
          image={image}
          x={0}
          y={0}
          width={width}
          height={width}
          fit="cover"
        />
      </Canvas>
    </View>
  );
};
