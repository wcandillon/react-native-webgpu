import * as THREE from "three";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { StyleSheet, Text, View } from "react-native";

import { useGLTF, useRGBE } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

export const Helmet = () => {
  const texture = useRGBE(require("./assets/helmet/royal_esplanade_1k.hdr"));
  const gltfScene = useGLTF(require("./assets/helmet/DamagedHelmet.gltf"));

  const ref = useCanvasEffect(async () => {
    if (!texture || !gltfScene) {
      return;
    }
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;

    const clock = new THREE.Clock();

    const camera = new THREE.PerspectiveCamera(45, width / height, 0.25, 20);
    camera.position.set(-1.8, 0.6, 2.7);

    const scene = new THREE.Scene();

    const renderer = makeWebGPURenderer(context);

    renderer.toneMapping = THREE.ACESFilmicToneMapping;

    texture.mapping = THREE.EquirectangularReflectionMapping;

    scene.background = texture;
    scene.environment = texture;

    scene.add(gltfScene);
    renderer.setAnimationLoop(animate);

    //
    function animateCamera() {
      const elapsed = clock.getElapsedTime();
      const distance = 5;
      camera.position.x = Math.sin(elapsed) * distance;
      camera.position.z = Math.cos(elapsed) * distance;
      camera.lookAt(new THREE.Vector3(0, 0, 0));
    }

    function animate() {
      animateCamera();
      renderer.render(scene, camera);
      context.present();
    }

    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [texture, gltfScene]);

  return (
    <View style={{ flex: 1, justifyContent: "center", alignItems: "center" }}>
      <Text>Loading assets...</Text>
      <View style={StyleSheet.absoluteFill}>
        <Canvas ref={ref} style={{ flex: 1 }} />
      </View>
    </View>
  );
};
