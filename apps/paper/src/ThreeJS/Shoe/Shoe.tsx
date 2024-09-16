import React, { useRef } from "react";
import { View, Image } from "react-native";
import * as THREE from "three";
import { GLTFLoader } from "GLTFLoader";

import { FiberCanvas } from "../components/FiberCanvas";
import useControls from "../components/OrbitControl";
import { useLoader } from "@react-three/fiber";


const modelUrl = Image.resolveAssetSource(
  require("../assets/jordan_shoe.glb")
  ).uri;

// function buildGraph(object: THREE.Object3D) {
//   const data: ObjectMap = { nodes: {}, materials: {} }
//   if (object) {
//     object.traverse((obj: any) => {
//       if (obj.name) data.nodes[obj.name] = obj
//       if (obj.material && !data.materials[obj.material.name]) data.materials[obj.material.name] = obj.material
//     })
//   }
//   return data
// }

// const useGLTF = (asset: string) => {
//   const [model, setModel] = useState<any | null>(null);
//   useEffect(() => {
//     const loader = new GLTFLoader(manager);
//     const dracoLoader = new DRACOLoader();
//     loader.setDRACOLoader(dracoLoader);
//     // eslint-disable-next-line @typescript-eslint/no-explicit-any
//     loader.load(asset, function (data: any) {
//       if (data.scene) {
//         Object.assign(data, buildGraph(data.scene));
//       }
//       console.log({ data: data.nodes });
//       setModel(data);
//     });
//   }, []);
//   return model;
// };

const Shoe = () => {
  const ref = useRef<THREE.Group>(null!);
  const gltf = useLoader(GLTFLoader, modelUrl);

  if (!gltf) {
    return null;
  }
  console.log(gltf);
  return  <group ref={ref}>
  <primitive object={gltf.scene} />
</group>;
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
