import * as THREE from "three";
import { Dimensions, View } from "react-native";
import { useRef } from "react";
import { useFrame } from "@react-three/fiber";

import { FiberCanvas } from "./components/FiberCanvas";
import useControls from "./components/OrbitControl";

interface BoxProps {
  position: [number, number, number];
}

function Box(props: BoxProps) {
  // This reference gives us direct access to the THREE.Mesh object
  const ref = useRef<THREE.Mesh>(null!);
  // Subscribe this component to the render-loop, rotate the mesh every frame
  useFrame((_state, delta) => (ref.current.rotation.x += delta));
  // Return the view, these are regular Threejs elements expressed in JSX
  return (
    <mesh {...props} ref={ref}>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color={"hotpink"} />
    </mesh>
  );
}

interface SceneProps {
}

const Scene = ({}: SceneProps) => {
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
      <Box position={[-1.2, 0, 0]} />
      <Box position={[1.2, 0, 0]} />
    </>
  );
};

export const Fiber = () => {
  const [OrbitControls, events] = useControls();
  return (
    <View style={{ flex: 1 }} {...events}>
      <FiberCanvas style={{ flex: 1 }}>
        <OrbitControls />
        <Scene />
      </FiberCanvas>
    </View>
  );
};
