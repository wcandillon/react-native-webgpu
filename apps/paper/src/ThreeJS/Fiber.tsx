import { View } from "react-native";
import { useRef } from "react";
import { useFrame } from "@react-three/fiber";

import { FiberCanvas } from "./components/FiberCanvas";

const Scene = () => {
  const meshRef = useRef();
  useFrame((state, delta) => (meshRef.current.rotation.x += delta));

  return (
    <>
      <ambientLight intensity={Math.PI / 2} />
      <spotLight
        position={[10, 10, 10]}
        angle={0.15}
        penumbra={1}
        decay={0}
        intensity={Math.PI}
      />
      <pointLight position={[-10, -10, -10]} decay={0} intensity={Math.PI} />
      <mesh ref={meshRef}>
        <boxGeometry args={[0.2, 0.2, 0.2]} />
        <meshStandardMaterial color="hotpink" />
      </mesh>
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
