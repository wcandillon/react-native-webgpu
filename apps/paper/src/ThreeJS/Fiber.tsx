import * as THREE from "three";
import { Dimensions, View } from "react-native";
import type { MutableRefObject, RefObject } from "react";
import { Ref, useEffect, useMemo, useRef, useState } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import { GLTFLoader } from "GLTFLoader";

import { FiberCanvas } from "./components/FiberCanvas";
import { manager } from "./assets/AssetManager";
const { width, height } = Dimensions.get("window");
const NUM_SPHERES = 0; // Number of spheres in the scene

const {
  float,
  vec3,
  color,
  viewportSharedTexture,
  checker,
  uv,
  timerLocal,
  oscSine,
  output,
  posterize,
  hue,
  grayscale,
  saturation,
  overlay,
  viewportUV,
  viewportSafeUV,
} = THREE;

interface SphereProps {
  index: number;
  numSpheres: number;
}

const Sphere = ({ index, numSpheres }: SphereProps) => {
  const meshRef = useRef<THREE.Mesh>(null);

  useFrame((state) => {
    const elapsed = state.clock.getElapsedTime();

    // Update sphere's position
    const angle = (elapsed + index) * 0.5;
    const radius = 3 + Math.sin(elapsed + index);
    meshRef.current!.position.set(
      Math.cos(angle) * radius,
      Math.sin(elapsed + index * 0.5),
      Math.sin(angle) * radius,
    );

    // Update sphere's color
    meshRef.current!.material.color.setHSL(
      (elapsed * 0.1 + index / numSpheres) % 1,
      0.7,
      0.5,
    );
  });

  return (
    <mesh ref={meshRef}>
      <sphereGeometry args={[0.5, 32, 32]} />
      <meshStandardMaterial color="white" />
    </mesh>
  );
};

interface SceneProps {
}

const Scene = ({}: SceneProps) => {
  const { camera, scene } = useThree();
  const mixer = useRef<THREE.AnimationMixer>();

  useEffect(() => {
    const loader = new GLTFLoader(manager);
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    loader.load("models/gltf/Michelle.glb", function (gltf: any) {
      const object = gltf.scene;
      mixer.current = new THREE.AnimationMixer(object);

      // eslint-disable-next-line prefer-destructuring
      const { material } = object.children[0].children[0];

      material.outputNode = oscSine(timerLocal(0.1)).mix(
        output,
        posterize(output.add(0.1), 4).mul(2),
      );

      const action = mixer.current.clipAction(gltf.animations[0]);
      action.play();
      scene.backgroundNode = viewportUV.y.mix(color(0x66bbff), color(0x4466ff));
      scene.add(object);
    });
  }, [mixer, scene]);

  useFrame((state, delta) => {
    const elapsed = state.clock.getElapsedTime();
    if (mixer.current) {
      mixer.current.update(delta);
    }

    // Update camera position to rotate around the scene
    const distance = 10;
    camera.position.x = Math.sin(elapsed * 0.2) * distance;
    camera.position.z = Math.cos(elapsed * 0.2) * distance;
    camera.lookAt(new THREE.Vector3(0, 0, 0));
  });

  return (
    <>
      <spotLight args={[0xffffff, 1]} power={2000} />
      {Array.from({ length: NUM_SPHERES }).map((_, i) => (
        <Sphere key={i} index={i} numSpheres={NUM_SPHERES} />
      ))}
    </>
  );
};

export const Fiber = () => {
  return (
    <View style={{ flex: 1 }}>
      <FiberCanvas style={{ flex: 1 }}>
        <Scene />
      </FiberCanvas>
    </View>
  );
};
