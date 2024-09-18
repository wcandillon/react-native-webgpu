import React, { useRef } from "react";
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
        <ambientLight intensity={0.7} />
        <spotLight
          intensity={0.5}
          angle={0.1}
          penumbra={1}
          position={[10, 15, 10]}
          castShadow
        />
        <Shoe />
      </FiberCanvas>
    </View>
  );
};
