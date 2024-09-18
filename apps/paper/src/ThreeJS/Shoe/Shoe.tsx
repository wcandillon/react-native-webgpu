import React, { useRef, useEffect } from "react";
import { View } from "react-native";
import * as THREE from "three";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { useGLTF } from "../assets/AssetManager";

const Shoe = () => {
  const ref = useRef<THREE.Group>(null!);
  const gltf = useGLTF(require("../assets/jordan/jordan_shoe.glb"));

  useEffect(() => {
    if (gltf) {
      // Center the model
      const box = new THREE.Box3().setFromObject(gltf.scene);
      const center = box.getCenter(new THREE.Vector3());
      gltf.scene.position.x -= center.x;
      gltf.scene.position.y -= center.y;
      gltf.scene.position.z -= center.z;

      // Assign different colors to each mesh
      let colorIndex = 0;
      const colors = [
        0xff0000, // Red
        0x00ff00, // Green
        0x0000ff, // Blue
        0xffff00, // Yellow
        0x00ffff, // Cyan
        0xff00ff, // Magenta
        0x3366ff, // White
        0x000000, // Black
      ];

      gltf.scene.traverse((child) => {
        if (child.isMesh) {
          // Clone the existing material to avoid affecting other meshes
          child.material = child.material.clone();
          // Set a new color from the colors array
          child.material.color.setHex(colors[colorIndex % colors.length]);
          colorIndex++;
        }
      });
    }
  }, [gltf]);

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
        <ambientLight intensity={0.5} />
        <directionalLight intensity={1} position={[5, 10, 7.5]} castShadow />
        <Shoe />
      </FiberCanvas>
    </View>
  );
};
