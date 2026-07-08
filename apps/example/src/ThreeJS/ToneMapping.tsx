import * as THREE from "three";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";
import {
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import { useEffect, useRef, useState } from "react";

import { useGLTF, useRGBE } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

const toneMappingOptions = {
  None: THREE.NoToneMapping,
  Linear: THREE.LinearToneMapping,
  Reinhard: THREE.ReinhardToneMapping,
  Cineon: THREE.CineonToneMapping,
  ACESFilmic: THREE.ACESFilmicToneMapping,
  AgX: THREE.AgXToneMapping,
  Neutral: THREE.NeutralToneMapping,
} as const;

type ToneMappingOption = keyof typeof toneMappingOptions;

export const ToneMapping = () => {
  const texture = useRGBE(require("./assets/venice/venice_sunset_1k.hdr"));
  const gltf = useGLTF(require("./assets/venice/venice_mask.gltf"));
  const ref = useRef<CanvasRef>(null);
  const rendererRef = useRef<ReturnType<typeof makeWebGPURenderer> | null>(
    null,
  );
  const [toneMapping, setToneMapping] = useState<ToneMappingOption>("Neutral");

  useEffect(() => {
    if (!texture || !gltf) {
      return;
    }
    const context = ref.current?.getContext("webgpu")!;
    const { width, height } = context.canvas;

    const clock = new THREE.Clock();

    const camera = new THREE.PerspectiveCamera(45, width / height, 0.01, 10);
    camera.position.set(-0.02, 0.03, 0.05);

    const scene = new THREE.Scene();
    scene.backgroundBlurriness = 0.3;

    // simulate sun
    const light = new THREE.DirectionalLight(0xfff3ee, 3);
    light.position.set(1, 0.05, 0.7);
    scene.add(light);

    const renderer = makeWebGPURenderer(context);
    renderer.toneMappingExposure = 1.0;
    rendererRef.current = renderer;

    texture.mapping = THREE.EquirectangularReflectionMapping;

    scene.background = texture;
    scene.environment = texture;

    scene.add(gltf.scene);
    renderer.setAnimationLoop(animate);

    const target = new THREE.Vector3(0, 0.03, 0);
    const distance = 0.055;

    function animateCamera() {
      const elapsed = clock.getElapsedTime();
      const theta = Math.sin(elapsed * 0.4) * 0.8 - 0.35;
      camera.position.set(
        target.x + Math.sin(theta) * distance,
        target.y + 0.005,
        target.z + Math.cos(theta) * distance,
      );
      camera.lookAt(target);
    }

    function animate() {
      animateCamera();
      renderer.render(scene, camera);
      context.present();
    }

    return () => {
      renderer.setAnimationLoop(null);
      rendererRef.current = null;
    };
  }, [texture, gltf, ref]);

  useEffect(() => {
    const renderer = rendererRef.current;
    if (renderer) {
      renderer.toneMapping = toneMappingOptions[toneMapping];
    }
  }, [toneMapping, texture, gltf]);

  return (
    <View style={styles.container}>
      <Text>Loading assets...</Text>
      <View style={StyleSheet.absoluteFill}>
        <Canvas ref={ref} style={styles.canvas} />
      </View>
      <View style={styles.picker}>
        <ScrollView horizontal showsHorizontalScrollIndicator={false}>
          {Object.keys(toneMappingOptions).map((name) => (
            <TouchableOpacity
              key={name}
              style={[
                styles.option,
                toneMapping === name && styles.selectedOption,
              ]}
              onPress={() => setToneMapping(name as ToneMappingOption)}
            >
              <Text
                style={[
                  styles.optionLabel,
                  toneMapping === name && styles.selectedOptionLabel,
                ]}
              >
                {name}
              </Text>
            </TouchableOpacity>
          ))}
        </ScrollView>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: "center",
    alignItems: "center",
  },
  canvas: {
    flex: 1,
  },
  picker: {
    position: "absolute",
    bottom: 48,
    left: 0,
    right: 0,
    paddingHorizontal: 16,
  },
  option: {
    backgroundColor: "rgba(0, 0, 0, 0.5)",
    borderRadius: 16,
    paddingHorizontal: 12,
    paddingVertical: 6,
    marginRight: 8,
  },
  selectedOption: {
    backgroundColor: "white",
  },
  optionLabel: {
    color: "white",
    fontSize: 13,
  },
  selectedOptionLabel: {
    color: "black",
  },
});
