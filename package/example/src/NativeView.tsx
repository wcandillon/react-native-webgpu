import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import { gpu, WebGPUView, WebGPUViewRef } from "react-native-webgpu";

export const NativeView = () => {
  const ref = useRef<WebGPUViewRef>(null);
  
  async function demo() {
    const adapter = await gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = gpu.getPreferredCanvasFormat();

    const context = ref.current?.getContext("webgpu");
    if (!context) {
      throw new Error("No context");
    }
    console.log(context)
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: 'premultiplied',
    });
    console.log(presentationFormat);
  }

  useEffect(() => {
    demo();
  }, [ref]);

  return <View style={style.container}>
    <WebGPUView ref={ref} style={style.webgpu} />
  </View>;
};

const style = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: "center",
    alignItems: "center",
  },
  webgpu: {
    width: 200,
    height: 200,
    justifyContent: "center",
    alignItems: "center",
  },
});