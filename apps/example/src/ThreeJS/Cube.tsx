import * as THREE from "three";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { View } from "react-native";

import { makeWebGPURenderer } from "./components/makeWebGPURenderer";
import CubeSceneSrc from "./CubeSceneSrc";
import { useEffect } from "react";
import { runOnUI, useAnimatedReaction } from "react-native-reanimated";
import { useClock } from "@shopify/react-native-skia";

const gpu = navigator.gpu;
const RNWebGPU = global.RNWebGPU;
const GPUBufferUsage = global.GPUBufferUsage;
const GPUColorWrite = global.GPUColorWrite;
const GPUMapMode = global.GPUMapMode;
const GPUShaderStage = global.GPUShaderStage;
const GPUTextureUsage = global.GPUTextureUsage;


const fibonacci = (num: number) => {
  let a = 1,
    b = 0,
    temp;

  while (num >= 0) {
    temp = a;
    a = a + b;
    b = temp;
    num--;
  }

  return b;
};

export const useMakeJsThreadBusy = () =>
  useEffect(() => {
    setInterval(() => {
      console.log("JS thread is busy now");
      while (true) {
        fibonacci(10000);
      }
    }, 2000);
  }, []);

export const Cube = () => {
  const clock = useClock();
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;
    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();
    runOnUI(() => {
      navigator.gpu = gpu;
      global.context = context;
      global.RNWebGPU = RNWebGPU;
      global.GPUBufferUsage = GPUBufferUsage;
      global.GPUColorWrite = GPUColorWrite;
      global.GPUMapMode = GPUMapMode;
      global.GPUShaderStage = GPUShaderStage;
      global.GPUTextureUsage = GPUTextureUsage;
      global.setImmediate = requestAnimationFrame;
      global.self = global;
  

      eval(CubeSceneSrc);
      global.renderCubeScene(context, device);
   })();
   
    // const camera = new THREE.PerspectiveCamera(70, width / height, 0.01, 10);
    // camera.position.z = 1;

    // const scene = new THREE.Scene();

    // const geometry = new THREE.BoxGeometry(0.2, 0.2, 0.2);
    // const material = new THREE.MeshNormalMaterial();

    // const mesh = new THREE.Mesh(geometry, material);
    // scene.add(mesh);

    // const renderer = makeWebGPURenderer(context, device);
    // renderer.init();
    // console.log(renderer._initialized);
    // function animate(time: number) {
    //   mesh.rotation.x = time / 2000;
    //   mesh.rotation.y = time / 1000;

    //   renderer.render(scene, camera);
    //   context.present();
    // }
    // renderer.setAnimationLoop(animate);
    // return () => {
    //   renderer.setAnimationLoop(null);
    // };
  });

  useAnimatedReaction(() => clock.value, (time) => {
   // function animate(time: number) {
   if (!global.renderer || !global.renderer._initialized) {
    return;
  }
    global.mesh.rotation.x = time / 2000;
    global.mesh.rotation.y = time / 1000;

      global.renderer.render(global.scene, global.camera);
      global.context.present();
  });
  
  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
