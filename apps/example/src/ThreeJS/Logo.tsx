import * as THREE from "three";
import { Canvas, useGPUContext } from "react-native-wgpu";
import { StyleSheet, View } from "react-native";
import { useEffect } from "react";

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

export const Logo = () => {
  const gltf = useGLTF(require("./assets/logo/scene.gltf"));
  const { ref, context } = useGPUContext();
  useEffect(() => {
    if (!gltf || !context) {
      return;
    }
    const { width, height } = context.canvas;

    const clock = new THREE.Clock();

    const camera = new THREE.PerspectiveCamera(45, width / height, 0.25, 20);
    camera.position.set(-1.8, 0.6, 5);

    const scene = new THREE.Scene();

    // Add lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
    scene.add(ambientLight);

    // Add directional light for highlights and shadows
    const directionalLight = new THREE.DirectionalLight(0xffffff, 1.5);
    directionalLight.position.set(5, 10, 7.5);
    scene.add(directionalLight);

    // Add secondary fill light from the opposite side
    const fillLight = new THREE.DirectionalLight(0xffffff, 0.5);
    fillLight.position.set(-5, 0, -5);
    scene.add(fillLight);

    const renderer = makeWebGPURenderer(context);

    scene.add(gltf.scene);
    renderer.setAnimationLoop(animate);

    //
    function animateCamera() {
      const elapsed = clock.getElapsedTime();
      const distance = 13;
      camera.position.x = Math.sin(elapsed) * distance;
      camera.position.z = Math.cos(elapsed) * distance;
      camera.lookAt(new THREE.Vector3(0, 0, 0));
    }

    function animate() {
      animateCamera();
      renderer.render(scene, camera);
      context!.present();
    }

    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [gltf, context]);

  return (
    <View style={{ flex: 1, backgroundColor: "rgb(60, 120, 255)" }}>
      <View style={StyleSheet.absoluteFill}>
        <Canvas ref={ref} style={{ flex: 1 }} />
      </View>
    </View>
  );
};
