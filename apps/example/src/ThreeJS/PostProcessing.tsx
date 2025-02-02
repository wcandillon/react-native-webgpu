import * as THREE from "three";
import { Canvas, useGPUContext } from "react-native-wgpu";
import { PixelRatio, Text, View, StyleSheet } from "react-native";
import { useEffect } from "react";
import { color, pass } from "three/tsl";
import { bloom } from "three/addons/tsl/display/BloomNode";

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

export const PostProcessing = () => {
  const gltf = useGLTF(require("./assets/PrimaryIonDrive.glb"));
  const { ref, context } = useGPUContext();
  useEffect(() => {
    if (!gltf || !context) {
      return;
    }
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();

    const { width, height } = context.canvas;

    const camera = new THREE.PerspectiveCamera(40, width / height, 1, 100);
    camera.position.set(-5, -8, -3.5);
    camera.lookAt(0, 0, 0);

    const scene = new THREE.Scene();
    scene.backgroundNode = color(0);
    camera.lookAt(0, 1, 0);

    const clock = new THREE.Clock();

    //lights

    const light = new THREE.SpotLight(0xffffff, 1);
    light.power = 2000;
    camera.add(light);
    scene.add(camera);

    const object = gltf.scene;
    const mixer = new THREE.AnimationMixer(object);

    const action = mixer.clipAction(gltf.animations[0]);
    action.play();

    scene.add(object);

    // portals
    //renderer
    const renderer = makeWebGPURenderer(context, { antialias: false });
    renderer.setAnimationLoop(animate);
    renderer.toneMapping = THREE.NeutralToneMapping;
    renderer.toneMappingExposure = 0.3;
    const postProcessing = new THREE.PostProcessing(renderer);

    const scenePass = pass(scene, camera);
    const scenePassColor = scenePass.getTextureNode("output");

    const bloomPass = bloom(scenePassColor);

    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    postProcessing.outputNode = scenePassColor.add(bloomPass);

    function animate() {
      const delta = clock.getDelta();

      if (mixer) {
        mixer.update(delta);
      }
      postProcessing.render();
      context!.present();
    }
    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [gltf, context]);
  return (
    <View style={{ flex: 1, justifyContent: "center", alignItems: "center" }}>
      <Text>Loading assets...</Text>
      <View style={StyleSheet.absoluteFill}>
        <Canvas ref={ref} style={{ flex: 1 }} />
      </View>
    </View>
  );
};
