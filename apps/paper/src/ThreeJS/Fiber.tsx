import { View } from "react-native";
import { useRef, useState } from "react";

import { FiberCanvas } from "./components/FiberCanvas";
import * as THREE from 'three/webgpu';

window.parent = window;

function TorusKnot() {
  const [hovered, hover] = useState(false);
  const ref = useRef<THREE.Mesh>(null!);
  // useFrame((state) => {
  //   const t = state.clock.elapsedTime / 2;
  //   ref.current.rotation.set(t, t, t);
  // });
  return (
    <mesh
      ref={ref}
      onPointerOver={() => hover(true)}
      onPointerOut={() => hover(false)}
    >
      <torusKnotGeometry args={[10, 3, 128, 16]} />
      <meshBasicMaterial color={hovered ? "orange" : "hotpink"} />
    </mesh>
  );
}

export const Helmet = () => {
  return (
    <View style={{ flex: 1 }}>
      <FiberCanvas style={{ flex: 1 }}>
        <color attach="background" args={["#dedddf"]} />
        <TorusKnot />
      </FiberCanvas>
    </View>
  );
};
