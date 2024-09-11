import type { NativeCanvas } from "react-native-wgpu";
import * as THREE from "three/webgpu";

// Here we need to wrap the Canvas into a non-host object for now
class ReactNativeCanvas {
  constructor(private canvas: NativeCanvas) {}

  get width() {
    return this.canvas.width;
  }

  get height() {
    return this.canvas.height;
  }

  get clientWidth() {
    return this.canvas.width;
  }

  get clientHeight() {
    return this.canvas.height;
  }

  set clientWidth(width: number) {
    this.canvas.width = width;
  }

  set clientHeight(height: number) {
    this.canvas.height = height;
  }
}

export const makeWebGPURenderer = (
  context: GPUCanvasContext,
  { antialias = true }: { antialias?: boolean } = {},
) =>
  new THREE.WebGPURenderer({
    antialias,
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    canvas: new ReactNativeCanvas(context.canvas),
    context,
  });
