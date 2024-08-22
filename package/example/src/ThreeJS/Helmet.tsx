import * as THREE from "three/webgpu";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import { StyleSheet, Text, View } from "react-native";
import { useRef } from "react";
import { GLTFLoader, RGBELoader } from "three-stdlib";

import { useCanvasEffect } from "../components/useCanvasEffect";

import { manager } from "./assets/AssetManager";

window.parent = window;

export const Helmet = () => {
  const ref = useRef<CanvasRef>(null);
  useCanvasEffect(async ({ device }) => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;
    let camera, scene, renderer;

    const clock = new THREE.Clock();
    init();

    function init() {
      camera = new THREE.PerspectiveCamera(45, width / height, 0.25, 20);
      camera.position.set(-1.8, 0.6, 2.7);

      scene = new THREE.Scene();

      new RGBELoader(manager)
        .setPath("textures/equirectangular/")
        .load("royal_esplanade_1k.hdr", function (texture) {
          texture.mapping = THREE.EquirectangularReflectionMapping;

          scene.background = texture;
          scene.environment = texture;

          const loader = new GLTFLoader(manager).setPath(
            "models/gltf/DamagedHelmet/glTF/",
          );
          loader.load("DamagedHelmet.gltf", function (gltf) {
            console.log("helmet loaded");
            scene.add(gltf.scene);

            render();
          });
        });

      renderer = new THREE.WebGPURenderer({
        antialias: true,
        canvas: context.canvas,
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-expect-error
        context,
        device,
      });

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

    function render() {
      animateCamera();
      renderer.renderAsync(scene, camera).then(() => {
        context.present();
        requestAnimationFrame(render);
      });
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
