import type { NativeCanvas } from "react-native-webgpu";
import * as THREE from "three/webgpu";

export class ReactNativeCanvas {
  constructor(private canvas: NativeCanvas) {}

  get width() {
    return this.canvas.width;
  }
  set width(v: number) {
    this.canvas.width = v;
  }
  get height() {
    return this.canvas.height;
  }
  set height(v: number) {
    this.canvas.height = v;
  }
  get clientWidth() {
    return this.canvas.clientWidth;
  }
  get clientHeight() {
    return this.canvas.clientHeight;
  }

  addEventListener() {}
  removeEventListener() {}
  dispatchEvent() {
    return false;
  }
  setPointerCapture() {}
  releasePointerCapture() {}
}

export function makeWebGPURenderer(context: GPUCanvasContext) {
  return new THREE.WebGPURenderer({
    antialias: true,
    canvas: new ReactNativeCanvas(
      context.canvas as unknown as NativeCanvas,
    ) as unknown as HTMLCanvasElement,
    context,
  });
}
