import * as THREE from "three";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";
import { StyleSheet, Text, View } from "react-native";
import { useEffect, useRef, useState } from "react";
import { RectButton } from "react-native-gesture-handler";

import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

declare const HermesInternal:
  | { getInstrumentedStats?: () => Record<string, number> }
  | undefined;

const getExternalMB = () => {
  const stats =
    typeof HermesInternal !== "undefined"
      ? HermesInternal?.getInstrumentedStats?.()
      : undefined;
  return (stats?.js_externalBytes ?? 0) / (1024 * 1024);
};

const forceGC = () => {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const { gc } = global as any;
  if (typeof gc === "function") {
    gc();
    return;
  }
  // Fallback: allocate short-lived garbage so Hermes runs a collection.
  let junk: number[][] = [];
  for (let i = 0; i < 50; i++) {
    junk.push(new Array(100000).fill(i));
  }
  junk = [];
};

// Big enough for a leak to be obvious: a 2048x2048 texture (16MB) on top of
// the renderer's own render targets.
const TEXTURE_SIZE = 2048;

// Regression demo for https://github.com/wcandillon/react-native-webgpu/issues/445
// The cleanup below intentionally only stops the animation loop, exactly like
// the issue repro: no renderer.dispose(), no device.destroy(). Three.js
// registers `device.lost.then(...)` which captures the whole renderer; the GC
// must still be able to reclaim the scene once this component unmounts.
const Scene = () => {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;

    const camera = new THREE.PerspectiveCamera(70, width / height, 0.01, 10);
    camera.position.z = 1;

    const scene = new THREE.Scene();

    const data = new Uint8Array(TEXTURE_SIZE * TEXTURE_SIZE * 4);
    for (let i = 0; i < data.length; i += 4) {
      data[i] = i % 255;
      data[i + 1] = (i / 4) % 255;
      data[i + 2] = 255 - (i % 255);
      data[i + 3] = 255;
    }
    const texture = new THREE.DataTexture(
      data,
      TEXTURE_SIZE,
      TEXTURE_SIZE,
      THREE.RGBAFormat,
    );
    texture.needsUpdate = true;

    const geometry = new THREE.BoxGeometry(0.4, 0.4, 0.4);
    const material = new THREE.MeshBasicMaterial({ map: texture });
    const mesh = new THREE.Mesh(geometry, material);
    scene.add(mesh);

    const renderer = makeWebGPURenderer(context);
    renderer.init();

    function animate(time: number) {
      mesh.rotation.x = time / 2000;
      mesh.rotation.y = time / 1000;
      renderer.render(scene, camera);
      context.present();
    }
    renderer.setAnimationLoop(animate);
    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [ref]);

  return <Canvas ref={ref} style={{ flex: 1 }} />;
};

export const Memory = () => {
  const [mounted, setMounted] = useState(true);
  const [cycles, setCycles] = useState(0);
  const [externalMB, setExternalMB] = useState(getExternalMB());
  useEffect(() => {
    const interval = setInterval(() => {
      forceGC();
      setExternalMB(getExternalMB());
    }, 1000);
    return () => clearInterval(interval);
  }, []);
  return (
    <View style={styles.container}>
      <View style={styles.panel}>
        <Text style={styles.stat}>
          js_externalBytes: {externalMB.toFixed(1)} MB
        </Text>
        <Text style={styles.caption}>
          Mount/unmount the scene repeatedly. The value should return close to
          its baseline a few seconds after each unmount (issue #445).
        </Text>
        <RectButton
          onPress={() => {
            if (mounted) {
              setCycles((c) => c + 1);
            }
            setMounted((m) => !m);
          }}
        >
          <View style={styles.button}>
            <Text style={styles.buttonLabel}>
              {mounted ? "Unmount scene" : `Mount scene (${cycles} cycles)`}
            </Text>
          </View>
        </RectButton>
      </View>
      <View style={styles.scene}>{mounted && <Scene />}</View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  panel: {
    padding: 16,
    gap: 8,
  },
  stat: {
    fontVariant: ["tabular-nums"],
    fontWeight: "bold",
  },
  caption: {
    color: "#666",
  },
  button: {
    backgroundColor: "white",
    padding: 16,
    borderBottomWidth: StyleSheet.hairlineWidth,
  },
  buttonLabel: {
    textAlign: "center",
  },
  scene: {
    flex: 1,
  },
});
