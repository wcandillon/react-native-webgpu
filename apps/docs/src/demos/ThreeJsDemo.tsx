"use client";

import { useEffect, useRef } from "react";
import { PixelRatio, StyleSheet, View } from "react-native";
import { Canvas, useDevice, type CanvasRef } from "react-native-webgpu";
import * as THREE from "three/webgpu";

import { makeWebGPURenderer } from "./makeWebGPURenderer";

export function ThreeJsDemo() {
  const { device } = useDevice();
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    if (!device) {
      return;
    }

    let animationId = 0;
    let cancelled = false;
    let renderer: THREE.WebGPURenderer | null = null;

    const start = async () => {
      const context = ref.current?.getContext("webgpu");
      if (!context || cancelled) {
        animationId = requestAnimationFrame(() => void start());
        return;
      }

      const canvas = context.canvas as HTMLCanvasElement;
      if (canvas.clientWidth === 0 || canvas.clientHeight === 0) {
        animationId = requestAnimationFrame(() => void start());
        return;
      }

      const dpr = PixelRatio.get();
      canvas.width = canvas.clientWidth * dpr;
      canvas.height = canvas.clientHeight * dpr;

      context.configure({
        device,
        format: navigator.gpu.getPreferredCanvasFormat(),
        alphaMode: "premultiplied",
      });

      renderer = makeWebGPURenderer(context);
      await renderer.init();

      const scene = new THREE.Scene();
      const camera = new THREE.PerspectiveCamera(
        70,
        canvas.clientWidth / canvas.clientHeight,
        0.01,
        10,
      );
      camera.position.z = 1;

      const mesh = new THREE.Mesh(
        new THREE.BoxGeometry(0.35, 0.35, 0.35),
        new THREE.MeshNormalMaterial(),
      );
      scene.add(mesh);

      const render = (time: number) => {
        if (cancelled || !renderer) {
          return;
        }

        mesh.rotation.x = time / 2000;
        mesh.rotation.y = time / 1000;
        renderer.render(scene, camera);
        context.present();
        animationId = requestAnimationFrame(render);
      };

      animationId = requestAnimationFrame(render);
    };

    void start();

    return () => {
      cancelled = true;
      cancelAnimationFrame(animationId);
      renderer?.setAnimationLoop(null);
      renderer?.dispose();
    };
  }, [device]);

  return (
    <View style={styles.container}>
      <Canvas ref={ref} style={StyleSheet.absoluteFill} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1 },
});
