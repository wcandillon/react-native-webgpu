import * as THREE from "three";
import type { CanvasRef, RNCanvasContext } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";
import { StyleSheet, Text, View } from "react-native";
import { useEffect, useRef, useState } from "react";
import { RectButton } from "react-native-gesture-handler";

import {
  makeWebGPURenderer,
  disposeWebGPURenderer,
} from "./components/makeWebGPURenderer";

declare const HermesInternal:
  { getInstrumentedStats?: () => Record<string, number> } | undefined;

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

// Schedules a deferred full GC + measurement. Isolated in its own function on
// purpose: Hermes closures share their enclosing environment, so a timeout
// created inside the test function would keep the test's locals (device,
// buffer) alive until it fires and pollute the measurement.
const measureLater = (label: string, before: number, allocated: number) => {
  setTimeout(() => {
    forceGC();
    const after = getExternalMB();
    console.log(
      `[Memory #445] ${label}: baseline=${before.toFixed(1)} MB, ` +
        `allocated=${allocated.toFixed(1)} MB, after gc()=${after.toFixed(1)} MB, ` +
        `reclaimed=${(allocated - after).toFixed(1)} MB`,
    );
  }, 500);
};

// Isolates the device.lost root from everything three.js does: allocates a
// 128MB buffer on a fresh device, optionally attaches the lost.then callback
// that used to root the graph (issue #445), drops every reference, then
// measures whether gc() reclaims the external bytes.
const runPureLostTest = async (withLostThen: boolean) => {
  const before = getExternalMB();
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    console.log("[Memory #445] no adapter");
    return;
  }
  {
    const device = await adapter.requestDevice();
    const buffer = device.createBuffer({
      size: 128 * 1024 * 1024,
      usage: GPUBufferUsage.VERTEX,
    });
    if (withLostThen) {
      device.lost.then((info) =>
        console.log("[Memory #445] lost fired", info.reason, buffer.size),
      );
      console.log(
        "[Memory #445] native fix present:",
        Object.getOwnPropertyNames(device).includes("__rnwgpuLostPromise"),
      );
    }
  }
  const allocated = getExternalMB();
  measureLater(
    withLostThen ? "pure test WITH lost.then" : "pure test without lost.then",
    before,
    allocated,
  );
};

// Big enough for a leak to be obvious: a 2048x2048 texture (16MB) on top of
// the renderer's own render targets.
const TEXTURE_SIZE = 2048;

// A raw WebGPU scene with the exact same lifecycle as the three.js scene
// below (fresh adapter + device per mount, big resources, a render loop, a
// `device.lost.then(...)` reaction capturing the whole scene, and NO
// device.destroy() on unmount), but zero three.js. With the #445 fix the GC
// must reclaim everything after unmount; contrasting it with the three.js
// scene isolates the remaining three.js-internal retention.
const pureSceneWGSL = /* wgsl */ `
  @group(0) @binding(0) var samp: sampler;
  @group(0) @binding(1) var tex: texture_2d<f32>;

  struct VOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
  };

  @vertex fn vs(@builtin(vertex_index) i: u32) -> VOut {
    var pos = array<vec2f, 3>(vec2f(-1, -3), vec2f(3, 1), vec2f(-1, 1));
    var out: VOut;
    out.pos = vec4f(pos[i], 0, 1);
    out.uv = pos[i] * vec2f(0.5, -0.5) + 0.5;
    return out;
  }

  @fragment fn fs(in: VOut) -> @location(0) vec4f {
    return textureSample(tex, samp, in.uv);
  }
`;

const startPureScene = async (context: RNCanvasContext) => {
  const adapter = await navigator.gpu.requestAdapter();
  const device = await adapter!.requestDevice();
  const format = navigator.gpu.getPreferredCanvasFormat();
  context.configure({ device, format, alphaMode: "premultiplied" });

  // Same footprint as the three.js scene: a 16MB texture, plus a large
  // vertex buffer so the leak (if any) is unmissable.
  const bigBuffer = device.createBuffer({
    size: 64 * 1024 * 1024,
    usage: GPUBufferUsage.VERTEX,
  });
  const data = new Uint8Array(TEXTURE_SIZE * TEXTURE_SIZE * 4);
  for (let i = 0; i < data.length; i += 4) {
    data[i] = i % 255;
    data[i + 1] = (i / 4) % 255;
    data[i + 2] = 255 - (i % 255);
    data[i + 3] = 255;
  }
  const texture = device.createTexture({
    size: [TEXTURE_SIZE, TEXTURE_SIZE],
    format: "rgba8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
  });
  device.queue.writeTexture(
    { texture },
    data,
    { bytesPerRow: TEXTURE_SIZE * 4 },
    { width: TEXTURE_SIZE, height: TEXTURE_SIZE },
  );
  const sampler = device.createSampler({ magFilter: "linear" });
  const module = device.createShaderModule({ code: pureSceneWGSL });
  const pipeline = device.createRenderPipeline({
    layout: "auto",
    vertex: { module, entryPoint: "vs" },
    fragment: { module, entryPoint: "fs", targets: [{ format }] },
    primitive: { topology: "triangle-list" },
  });
  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: sampler },
      { binding: 1, resource: texture.createView() },
    ],
  });

  // Mimic three's WebGPUBackend.init(): a lost reaction whose closure
  // captures the whole scene.
  device.lost.then((info) => {
    if (info.reason !== "destroyed") {
      console.log(
        "[Memory #445] pure scene device lost",
        info.reason,
        bigBuffer.size,
        bindGroup.label,
      );
    }
  });

  let rafId = 0;
  const render = () => {
    const encoder = device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view: context.getCurrentTexture().createView(),
          clearValue: [0, 0, 0, 1],
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.draw(3);
    pass.end();
    device.queue.submit([encoder.finish()]);
    context.present();
    rafId = requestAnimationFrame(render);
  };
  rafId = requestAnimationFrame(render);

  // Cleanup only stops the loop, mirroring the three.js scene: no
  // device.destroy(), the GC has to reclaim everything.
  return () => cancelAnimationFrame(rafId);
};

const PureScene = () => {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    let cleanup: (() => void) | null = null;
    let unmounted = false;
    startPureScene(ref.current!.getContext("webgpu")!).then((c) => {
      if (unmounted) {
        c();
      } else {
        cleanup = c;
      }
    });
    return () => {
      unmounted = true;
      cleanup?.();
    };
  }, [ref]);

  return <Canvas ref={ref} style={{ flex: 1 }} />;
};

// Regression demo for https://github.com/wcandillon/react-native-webgpu/issues/445
// The cleanup calls renderer.dispose() (required by three: it stops the
// renderer's internal requestAnimationFrame loop, which otherwise keeps the
// whole renderer alive; setAnimationLoop(null) alone does NOT stop it) but
// deliberately does NOT destroy the device. Three registers
// `device.lost.then(...)` which captures the whole renderer; the GC must be
// able to reclaim the scene, the device, and all GPU wrappers regardless.
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
      disposeWebGPURenderer(renderer);
    };
  }, [ref]);

  return <Canvas ref={ref} style={{ flex: 1 }} />;
};

type SceneKind = "three" | "pure" | null;

export const Memory = () => {
  const [scene, setScene] = useState<SceneKind>(null);
  const [externalMB, setExternalMB] = useState(getExternalMB());
  useEffect(() => {
    const interval = setInterval(() => {
      forceGC();
      setExternalMB(getExternalMB());
    }, 1000);
    return () => clearInterval(interval);
  }, []);
  // Post-unmount probe: give React a tick to detach the subtree, then force
  // a full GC and log whether the scene was reclaimed. Deferred on purpose:
  // a synchronous gc() at unmount time runs while the effect cleanup
  // closures still reference the scene.
  const toggle = (kind: Exclude<SceneKind, null>) => {
    if (scene === kind) {
      const before = getExternalMB();
      setTimeout(() => {
        forceGC();
        const after = getExternalMB();
        console.log(
          `[Memory #445] ${kind} scene unmount: ${before.toFixed(1)} MB -> ` +
            `after gc(): ${after.toFixed(1)} MB ` +
            `(reclaimed ${(before - after).toFixed(1)} MB)`,
        );
        setExternalMB(after);
      }, 500);
      setScene(null);
    } else {
      setScene(kind);
    }
  };
  return (
    <View style={styles.container}>
      <View style={styles.panel}>
        <Text style={styles.stat}>
          js_externalBytes: {externalMB.toFixed(1)} MB (gc():{" "}
          {/* eslint-disable-next-line @typescript-eslint/no-explicit-any */}
          {typeof (global as any).gc === "function"
            ? "available"
            : "MISSING, using churn fallback"}
          )
        </Text>
        <Text style={styles.caption}>
          Mount/unmount a scene repeatedly. The value should return close to its
          baseline a few seconds after each unmount (issue #445). The pure
          WebGPU scene has the same lifecycle without three.js, isolating
          three's internal retention from the library.
        </Text>
        <RectButton onPress={() => toggle("pure")}>
          <View style={styles.button}>
            <Text style={styles.buttonLabel}>
              {scene === "pure"
                ? "Unmount pure WebGPU scene"
                : "Mount pure WebGPU scene (no three.js)"}
            </Text>
          </View>
        </RectButton>
        <RectButton onPress={() => toggle("three")}>
          <View style={styles.button}>
            <Text style={styles.buttonLabel}>
              {scene === "three"
                ? "Unmount three.js scene"
                : "Mount three.js scene"}
            </Text>
          </View>
        </RectButton>
        <RectButton onPress={() => runPureLostTest(true)}>
          <View style={styles.button}>
            <Text style={styles.buttonLabel}>
              Pure test: 128MB buffer WITH device.lost.then
            </Text>
          </View>
        </RectButton>
        <RectButton onPress={() => runPureLostTest(false)}>
          <View style={styles.button}>
            <Text style={styles.buttonLabel}>
              Pure test: 128MB buffer without lost.then
            </Text>
          </View>
        </RectButton>
      </View>
      <View style={styles.scene}>
        {scene === "three" && <Scene />}
        {scene === "pure" && <PureScene />}
      </View>
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
