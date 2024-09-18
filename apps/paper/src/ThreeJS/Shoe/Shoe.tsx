import React, { useRef, useEffect } from "react";
import { View } from "react-native";
import * as THREE from "three";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { useGLTF } from "../assets/AssetManager";

const Shoe = () => {
  const ref = useRef<THREE.Group>(null!);
  const gltf = useGLTF(require("../assets/jordan/jordan_shoe.glb"));
  if (!gltf) {
    return null;
  }

  // Center the model
  const box = new THREE.Box3().setFromObject(gltf.scene);
  const center = box.getCenter(new THREE.Vector3());
  gltf.scene.position.x -= center.x;
  gltf.scene.position.y -= center.y;
  gltf.scene.position.z -= center.z;

  return (
    <group ref={ref}>
      <primitive object={gltf.scene} />
    </group>
  );
};

export const ShoeDemo = () => {
  const [OrbitControls, events] = useControls();
  return (
    <View style={{ flex: 1 }} {...events}>
      <FiberCanvas style={{ flex: 1 }}>
        <color attach="background" args={[0xffffff]} />

        <OrbitControls />
        <ambientLight intensity={0.5} />
        <directionalLight intensity={1} position={[5, 10, 7.5]} castShadow />
        <Shoe />
      </FiberCanvas>
    </View>
  );
};
