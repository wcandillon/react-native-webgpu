import * as THREE from "three";
import { Canvas, useGPUContext } from "react-native-wgpu";
import { StyleSheet, View } from "react-native";
import { useEffect, useRef } from "react";
import { Gesture, GestureDetector } from "react-native-gesture-handler";
import { useSharedValue } from "react-native-reanimated";

import {
  Matrix4,
  multiply4,
  scale,
  convertToColumnMajor,
  concat,
  rotate,
} from "../components/Matrix4";

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

export const Logo = () => {
  const gltf = useGLTF(require("./assets/logo/scene.gltf"));
  const { ref, context } = useGPUContext();
  const origin = useSharedValue({ x: 0, y: 0 });
  const offset = useSharedValue(Matrix4());
  const matrix = useSharedValue(Matrix4());
  const pan = Gesture.Pan().onChange((e) => {
    // Calculate rotation matrices
    const rx = e.changeY * 0.01;
    const ry = e.changeX * 0.01;
    matrix.value = concat(
      matrix.value,
      rotate([1, 0, 0], rx),
      rotate([0, 1, 0], ry),
    );
  });
  const pinch = Gesture.Pinch()
    .onBegin((e) => {
      origin.value = { x: e.focalX, y: e.focalY };
      offset.value = Matrix4();
      matrix.value = Matrix4();
    })
    .onChange((e) => {
      matrix.value = multiply4(
        offset.value,
        scale(e.scale, e.scale, 1, { x: 0, y: 0 }),
      );
    });

  useEffect(() => {
    if (!gltf || !context) {
      return;
    }
    const { width, height } = context.canvas;

    //const clock = new THREE.Clock();

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

    function animateCamera() {
      const distance = 12; // Distance from the center

      camera.position.set(0, 0, distance);
      camera.lookAt(new THREE.Vector3(0, 0, 0));
    }

    function animate() {
      // Apply rotation from gesture
      if (gltf) {
        const threeMatrix = new THREE.Matrix4();
        threeMatrix.fromArray(convertToColumnMajor(matrix.value));
        gltf.scene.matrix.copy(threeMatrix);
        gltf.scene.matrixAutoUpdate = false;
      }

      animateCamera();
      renderer.render(scene, camera);
      context!.present();
    }

    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [gltf, context, matrix]);
  const gesture = Gesture.Race(pinch, pan);
  return (
    <View style={{ flex: 1, backgroundColor: "rgb(60, 120, 255)" }}>
      <GestureDetector gesture={gesture}>
        <Canvas ref={ref} style={{ flex: 1 }} />
      </GestureDetector>
    </View>
  );
};
