import React, { useState } from "react";
import { View } from "react-native";
import * as THREE from "three";
import { GLTFLoader } from "GLTFLoader";
import { DRACOLoader } from "DRACOLoader";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { manager } from "../assets/AssetManager";

const useGLTF = (asset: string) => {
  const [model, setModel] = useState<THREE.Object3D | null>(null);
  const loader = new GLTFLoader(manager);
  const dracoLoader = new DRACOLoader();
  loader.setDRACOLoader(dracoLoader);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  loader.load(asset, function (gltf: any) {
    const object = gltf.scene;
    const result  = object.children[0].children[0];
    setModel(result);
  });
  return model;
};

const Shoe = () => {
  const model = useGLTF("jordan_shoe.gltf");
  console.log(!!model);
  return <group />;
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
