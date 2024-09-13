import * as THREE from "three";
import { Dimensions, View } from "react-native";
import { useRef } from "react";
import { useFrame, useThree } from "@react-three/fiber";

import { FiberCanvas } from "./components/FiberCanvas";

const { width, height } = Dimensions.get("window");

const NUM_SPHERES = 10; // Number of spheres in the scene

const Scene = () => {
  const sphereRefs = useRef([]);
  const { camera } = useThree();

  useFrame((state) => {
    const elapsed = state.clock.getElapsedTime();

    // Update camera position to rotate around the scene
    const distance = 10;
    camera.position.x = Math.sin(elapsed * 0.2) * distance;
    camera.position.z = Math.cos(elapsed * 0.2) * distance;
    camera.lookAt(new THREE.Vector3(0, 0, 0));

    // Update spheres' positions and colors
    sphereRefs.current.forEach((mesh, index) => {
      const angle = (elapsed + index) * 0.5;
      const radius = 3 + Math.sin(elapsed + index);
      mesh.position.set(
        Math.cos(angle) * radius,
        Math.sin(elapsed + index * 0.5),
        Math.sin(angle) * radius,
      );

      // Change color over time
      mesh.material.color.setHSL(
        (elapsed * 0.1 + index / NUM_SPHERES) % 1,
        0.7,
        0.5,
      );
    });
  });

  return (
    <>
      <ambientLight intensity={0.3} />
      <pointLight position={[0, 10, 0]} intensity={1.5} />
      {Array.from({ length: NUM_SPHERES }).map((_, i) => (
        <mesh
          key={i}
          ref={(el) => (sphereRefs.current[i] = el)}
          position={[0, 0, 0]}
        >
          <sphereGeometry args={[0.5, 32, 32]} />
          <meshStandardMaterial color="white" />
        </mesh>
      ))}
    </>
  );
};

export const Fiber = () => {
  const camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 100);
  camera.position.set(0, 5, 10);
  return (
    <View style={{ flex: 1 }}>
      <FiberCanvas style={{ flex: 1 }} camera={camera}>
        <Scene />
      </FiberCanvas>
    </View>
  );
};
