/* eslint-disable camelcase */
import React, { useRef, useEffect } from "react";
import { View } from "react-native";
import * as THREE from "three";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { useGLTF } from "../assets/AssetManager";

const colorMap = {
  nikelogo: 0xfe8cdd,
  lateralwhite: 0xffffff,
  upperlateral: 0x2e65fe,
  base_white: 0xffffff,
  upper: 0xffffff,
  base_red: 0xfe8cdd,
  Cylinder: 0x2e65fe,
  uppercap: 0x2e65fe,
  lateralcords_left_1: 0xffffff,
  lateralcords_right_1: 0xffffff,
  tongue: 0xffffff,
  redcollar: 0xfe8cdd,
  counterlinning: 0x2e65fe,
  counterlinninginside: 0x2e65fe,
  conterlinninginside: 0x2e65fe,
  logotag: 0x2e65fe,
  counter: 0xfe8cdd,
  shapetongueleft_1: "0xffffff",
  shapetongueright_1: "0xffffff",
};

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

      gltf.scene.traverse((child) => {
        if (child.isMesh) {
          // Replace the existing material with MeshStandardMaterial
          const material = new THREE.MeshStandardMaterial({
            color: 0xffffff, // Default color
            side: THREE.DoubleSide, // Render both sides
            roughness: 0.5, // Adjust as needed
            metalness: 0.1, // Adjust as needed
          });

          // Set the color based on your colorMap
          const key = child.name.split("_")[0];
          if (child.name.startsWith("cord")) {
            material.color.setHex(0xffffff);
          } else if (colorMap[child.name]) {
            material.color.setHex(colorMap[child.name]);
          } else if (colorMap[key]) {
            material.color.setHex(colorMap[key]);
          } else {
            console.log("No color found for:", child.name);
            material.color.setHex(0xff0000);
          }

          child.material = material;
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
