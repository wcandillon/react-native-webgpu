/* eslint-disable @typescript-eslint/ban-ts-comment */
import * as THREE from "three";
import { Canvas, useGPUContext } from "react-native-wgpu";
import { View } from "react-native";
import { useEffect } from "react";
import { time, oscSine, mix, range, normalWorld } from "three/tsl";

import { makeWebGPURenderer } from "./components/makeWebGPURenderer";
import { useGeometry } from "./assets/AssetManager";

export const InstancedMesh = () => {
  const geometry = useGeometry(
    "https://threejs.org/examples/models/json/suzanne_buffergeometry.json",
  );
  const { ref, context } = useGPUContext();
  useEffect(() => {
    if (!context || !geometry) {
      return;
    }
    const { width, height } = context.canvas;

    const amount = 10;
    const count = Math.pow(amount, 3);
    const dummy = new THREE.Object3D();

    const camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 100);
    camera.position.set(amount * 0.9, amount * 0.9, amount * 0.9);
    camera.lookAt(0, 0, 0);

    const scene = new THREE.Scene();

    const material = new THREE.MeshBasicMaterial();

    // random colors between instances from 0x000000 to 0xFFFFFF
    const randomColors = range(
      new THREE.Color(0x000000),
      new THREE.Color(0xffffff),
    );

    // @ts-expect-error
    material.colorNode = mix(normalWorld, randomColors, oscSine(time.mul(0.1)));

    console.log("geometry loaded");
    geometry.computeVertexNormals();
    geometry.scale(0.5, 0.5, 0.5);

    const mesh = new THREE.InstancedMesh(geometry, material, count);
    mesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);

    scene.add(mesh);

    //
    const renderer = makeWebGPURenderer(context!);

    //renderer.setPixelRatio(window.devicePixelRatio);
    //renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setAnimationLoop(animate);
    //document.body.appendChild(renderer.domElement);

    function animate() {
      render();
      context!.present();
    }

    function render() {
      if (mesh) {
        const t = Date.now() * 0.001;

        mesh.rotation.x = Math.sin(t / 4);
        mesh.rotation.y = Math.sin(t / 2);

        let i = 0;
        const offset = (amount - 1) / 2;

        for (let x = 0; x < amount; x++) {
          for (let y = 0; y < amount; y++) {
            for (let z = 0; z < amount; z++) {
              dummy.position.set(offset - x, offset - y, offset - z);
              dummy.rotation.y =
                Math.sin(x / 4 + t) + Math.sin(y / 4 + t) + Math.sin(z / 4 + t);
              dummy.rotation.z = dummy.rotation.y * 2;

              dummy.updateMatrix();
              mesh.setMatrixAt(i++, dummy.matrix);
            }
          }
        }
      }

      renderer.render(scene, camera);
    }
    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [context, geometry]);

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
