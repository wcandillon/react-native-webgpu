import React, { useEffect, useRef, useState } from "react";
import { View, Image } from "react-native";
import * as THREE from "three";
import { GLTFLoader } from "GLTFLoader";
import { DRACOLoader } from "DRACOLoader";
import { buildGraph, useThree } from "@react-three/fiber";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { manager } from "../assets/AssetManager";

const modelUrl = Image.resolveAssetSource(
  require("../assets/jordan_shoe.glb"),
).uri;

const useGLTF = (asset: string) => {
  const [scene, setScene] = useState<any | null>(null);
  useEffect(() => {
    const loader = new GLTFLoader(manager);
    const dracoLoader = new DRACOLoader();
    loader.setDRACOLoader(dracoLoader);
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    loader.load(asset, function (data: any) {
      setScene(data.scene);
    });
  }, []);
  return scene;
};

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
  const { camera } = useThree();
  const ref = useRef<THREE.Group>(null!);
  const gltf = useGLTF(modelUrl);
  useEffect(() => {
    // Move the camera closer to the shoe
    camera.position.set(0, 0, 2); // Adjust the Z value to zoom in or out

    // Ensure the camera looks at the shoe
    if (ref.current) {
      const shoePosition = new THREE.Vector3();
      ref.current.getWorldPosition(shoePosition);
      camera.lookAt(shoePosition);
    }
  }, [camera, ref]);
  if (!gltf) {
    return null;
  }
  const { nodes, materials } = buildGraph(gltf);
  return (
    <group ref={ref}>
      <group
        position={[0, -0.51, 0.29]}
        rotation={[Math.PI / 2, 0, -1.61]}
        scale={0.61}
      >
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive0.geometry}
          material={materials.side}
          material-color={snap.items.side}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive1.geometry}
          material={materials.side}
          material-color={snap.items.side}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive2.geometry}
          material={materials.side}
          material-color={snap.items.side}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive3.geometry}
          material={materials.side}
          material-color={snap.items.side}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive4.geometry}
          material={materials.back_flipper}
          material-color={snap.items.back_flipper}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive5.geometry}
          material={materials.front_down}
          material-color={snap.items.front_down}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive6.geometry}
          material={materials.slashes}
          material-color={snap.items.slashes}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive7.geometry}
          material={materials.mini_flaps}
          material-color={snap.items.mini_flaps}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive8.geometry}
          material={materials.front_side}
          material-color={snap.items.front_side}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive9.geometry}
          material={materials.side_flaps}
          material-color={snap.items.side_flaps}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive10.geometry}
          material={materials.back_flip}
          material-color={snap.items.back_flip}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive11.geometry}
          material={materials.logo}
          material-color={snap.items.logo}
        />

        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive12.geometry}
          material={materials.middle_sides}
          material-color={snap.items.middle_sides}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive13.geometry}
          material={materials.middle_sides}
          material-color={snap.items.middle_sides}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive14.geometry}
          material={materials.middle_sides}
          material-color={snap.items.middle_sides}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive15.geometry}
          material={materials.upper_side}
          material-color={snap.items.upper_side}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive16.geometry}
          material={materials.upper_soft}
          material-color={snap.items.upper_soft}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive17.geometry}
          material={materials.softy}
          material-color={snap.items.softy}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive18.geometry}
          material={materials.big_front}
          material-color={snap.items.big_front}
        />
        <mesh
          //receiveShadow
          castShadow
          geometry={nodes.Object001_primitive19.geometry}
          material={materials.upper_bottom_bottom}
          material-color={snap.items.upper_bottom_bottom}
        />
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
