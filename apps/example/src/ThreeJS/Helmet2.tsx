import * as THREE from "three";
import { Canvas, useGPUContext } from "react-native-wgpu";
import { StyleSheet, Text, View } from "react-native";
import { useEffect } from "react";
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
import { useMakeJsThreadBusy } from "../components/useMakeJSThreadBusy";

import { useGLTF, useRGBE } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

export const Helmet = () => {
  const texture = useRGBE(require("./assets/helmet/royal_esplanade_1k.hdr"));
  const gltf = useGLTF(require("./assets/helmet/DamagedHelmet.gltf"));
  const { ref, context } = useGPUContext();
  useMakeJsThreadBusy();

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
    if (!texture || !gltf || !context) {
      return;
    }
    const { width, height } = context.canvas;

    const camera = new THREE.PerspectiveCamera(45, width / height, 0.25, 20);
    camera.position.set(-1.8, 0.6, 2.7);

    const scene = new THREE.Scene();

    const renderer = makeWebGPURenderer(context);

    renderer.toneMapping = THREE.ACESFilmicToneMapping;

    texture.mapping = THREE.EquirectangularReflectionMapping;

    scene.background = texture;
    scene.environment = texture;

    scene.add(gltf.scene);
    renderer.setAnimationLoop(animate);

    const distance = 5;
    camera.position.z = distance;
    camera.lookAt(new THREE.Vector3(0, 0, 0));

    function animate() {
      // Apply transformations from gesture
      if (gltf) {
        const threeMatrix = new THREE.Matrix4();
        threeMatrix.fromArray(convertToColumnMajor(matrix.value));
        gltf.scene.matrix.copy(threeMatrix);
        gltf.scene.matrixAutoUpdate = false;
      }

      renderer.render(scene, camera);
      context!.present();
    }

    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [texture, gltf, context, matrix]);

  const gesture = Gesture.Race(pinch, pan);

  return (
    <View style={{ flex: 1, justifyContent: "center", alignItems: "center" }}>
      <Text>Loading assets...</Text>
      <View style={StyleSheet.absoluteFill}>
        <GestureDetector gesture={gesture}>
          <Canvas ref={ref} style={{ flex: 1 }} />
        </GestureDetector>
      </View>
    </View>
  );
};
