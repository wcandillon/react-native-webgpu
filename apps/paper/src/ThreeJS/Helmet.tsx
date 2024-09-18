/* eslint-disable @typescript-eslint/no-explicit-any */
import * as THREE from "three";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { StyleSheet, Text, View } from "react-native";
import { GLTFLoader } from "GLTFLoader";
import { RGBELoader } from "RGBELoader";

import { resolveAsset } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

export const Helmet = () => {
  const ref = useCanvasEffect(async () => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;
    let camera: THREE.Camera, scene: THREE.Scene, renderer: THREE.Renderer;

    const clock = new THREE.Clock();
    init();

    function init() {
      camera = new THREE.PerspectiveCamera(45, width / height, 0.25, 20);
      camera.position.set(-1.8, 0.6, 2.7);

      scene = new THREE.Scene();

      new RGBELoader().load(
        resolveAsset(require("./assets/helmet/royal_esplanade_1k.hdr")),
        function (texture: any) {
          texture.mapping = THREE.EquirectangularReflectionMapping;

          scene.background = texture;
          scene.environment = texture;

          const loader = new GLTFLoader();
          loader.load(
            resolveAsset(require("./assets/helmet/DamagedHelmet.gltf")),
            function (gltf: any) {
              console.log("helmet loaded");
              scene.add(gltf.scene);

              renderer.setAnimationLoop(animate);
            },
          );
        },
      );

      renderer = makeWebGPURenderer(context);

      renderer.toneMapping = THREE.ACESFilmicToneMapping;
    }

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
  });

  return (
    <View style={{ flex: 1, justifyContent: "center", alignItems: "center" }}>
      <Text>Loading assets...</Text>
      <View style={StyleSheet.absoluteFill}>
        <Canvas ref={ref} style={{ flex: 1 }} />
      </View>
    </View>
  );
};
