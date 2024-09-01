/* eslint-disable @typescript-eslint/ban-ts-comment */
import * as THREE from "three/src/Three.WebGPU";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { View } from "react-native";

import { manager } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

const { timerLocal, oscSine, mix, range } = THREE;

export const InstancedMesh = () => {
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;
    let camera: THREE.Camera, scene: THREE.Scene, renderer: THREE.Renderer;

    let mesh: THREE.InstancedMesh;
    const amount = 10;
    const count = Math.pow(amount, 3);
    const dummy = new THREE.Object3D();

    init();

    function init() {
      camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 100);
      camera.position.set(amount * 0.9, amount * 0.9, amount * 0.9);
      camera.lookAt(0, 0, 0);

      scene = new THREE.Scene();

      const material = new THREE.MeshBasicMaterial();

      // random colors between instances from 0x000000 to 0xFFFFFF
      const randomColors = range(
        new THREE.Color(0x000000),
        new THREE.Color(0xffffff),
      );

      // @ts-expect-error
      material.colorNode = mix(
        THREE.normalWorld,
        randomColors,
        oscSine(timerLocal(0.1)),
      );

      const loader = new THREE.BufferGeometryLoader(manager);
      loader.load(
        "models/json/suzanne_buffergeometry.json",
        function (geometry) {
          console.log("geometry loaded");
          geometry.computeVertexNormals();
          geometry.scale(0.5, 0.5, 0.5);

          mesh = new THREE.InstancedMesh(geometry, material, count);
          mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);

          scene.add(mesh);

          //
        },
      );

      //
      renderer = makeWebGPURenderer(context);

      //renderer.setPixelRatio(window.devicePixelRatio);
      //renderer.setSize(window.innerWidth, window.innerHeight);
      renderer.setAnimationLoop(animate);
      //document.body.appendChild(renderer.domElement);
    }

    async function animate() {
      await render();
      context.present();
    }

    async function render() {
      if (mesh) {
        const time = Date.now() * 0.001;

        mesh.rotation.x = Math.sin(time / 4);
        mesh.rotation.y = Math.sin(time / 2);

        let i = 0;
        const offset = (amount - 1) / 2;

        for (let x = 0; x < amount; x++) {
          for (let y = 0; y < amount; y++) {
            for (let z = 0; z < amount; z++) {
              dummy.position.set(offset - x, offset - y, offset - z);
              dummy.rotation.y =
                Math.sin(x / 4 + time) +
                Math.sin(y / 4 + time) +
                Math.sin(z / 4 + time);
              dummy.rotation.z = dummy.rotation.y * 2;

              dummy.updateMatrix();
              mesh.setMatrixAt(i++, dummy.matrix);
            }
          }
        }
      }

      await renderer.render(scene, camera);
    }
    return () => {
      renderer.setAnimationLoop(null);
    };
  });

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
