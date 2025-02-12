import { RNCanvasContext } from "react-native-wgpu";
import * as THREE from "three";
import * as THREE_TSL from "three/tsl";
import { makeWebGPURenderer } from "./makeWebGPURenderer";

declare global {
  var Three: typeof THREE;
  var ThreeTSL: typeof THREE_TSL;
  var makeThreeWebGPURenderer: (device: GPUDevice, context: RNCanvasContext) => THREE.WebGPURenderer;
}

global.Three = THREE;
global.ThreeTSL = THREE_TSL;
global.makeThreeWebGPURenderer = makeWebGPURenderer;
