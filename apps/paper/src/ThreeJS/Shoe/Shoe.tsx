import React, { useEffect, useRef } from "react";
import { View } from "react-native";
import * as THREE from "three";
import { buildGraph, useThree } from "@react-three/fiber";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { useGLTF } from "../assets/AssetManager";

const snap = {
  items: {
    side: "#e3e3e3",
    back_flipper: "#ffffff",
    front_down: "#e3e3e3",
    slashes: "#191a11",
    mini_flaps: "#3d3d3d",
    side_flaps: "#af1a2b",
    back_flip: "#af1a2b",
    logo: "#3a3a3a",
    upper_side: "#ad1b29",
    upper_soft: "#3a3a3a",
    softy: "#0e0f10",
    big_front: "#0e0f10",
    upper_bottom_bottom: "#d7d2d1",
    bottooom: "#7c1013",
    bottom_logo: "#3a3a3a",
    middle_sides: "#b01826",
    front_side: "#b31929",
  },
};

const Shoe = () => {
  const ref = useRef<THREE.Group>(null!);
  const gltf = useGLTF(require("../assets/jordan/jordan_shoe.glb"));
  if (!gltf) {
    return null;
  }
  return (
    <group ref={ref}>
      <group
        position={[0, -0.51, 0.29]}
        rotation={[Math.PI / 2, 0, -1.61]}
        scale={0.61}
      >
        <primitive object={gltf.scene} />
      </group>
    </group>
  );
};

export const ShoeDemo = () => {
  const [OrbitControls, events] = useControls();
  return (
    <View style={{ flex: 1 }} {...events}>
      <FiberCanvas style={{ flex: 1 }}>
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
