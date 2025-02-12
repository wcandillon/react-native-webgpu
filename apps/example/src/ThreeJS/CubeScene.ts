import * as THREE from "three";
import type { RNCanvasContext } from "react-native-wgpu";

import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

declare global {
  var renderCubeScene: (
    context: RNCanvasContext,
    device: GPUDevice,
  ) => Promise<() => void>;
}

global.renderCubeScene = async (
  context: RNCanvasContext,
  device: GPUDevice,
) => {
  const { width, height } = context.canvas;

  const camera = new THREE.PerspectiveCamera(70, width / height, 0.01, 10);
  camera.position.z = 1;

  const scene = new THREE.Scene();

  const geometry = new THREE.BoxGeometry(0.2, 0.2, 0.2);
  const material = new THREE.MeshNormalMaterial();

  const mesh = new THREE.Mesh(geometry, material);
  scene.add(mesh);

  const renderer = makeWebGPURenderer(context, device);
  console.log("Before init()");
  await renderer.init();
  console.log("After init()");

  function animate(time: number) {
    mesh.rotation.x = time / 2000;
    mesh.rotation.y = time / 1000;

    renderer.render(scene, camera);
    context.present();
  }
  renderer.setAnimationLoop(animate);
  return () => {
    renderer.setAnimationLoop(null);
  };
};
