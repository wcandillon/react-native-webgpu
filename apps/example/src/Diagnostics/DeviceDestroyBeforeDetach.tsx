import React, { useEffect, useRef, useState } from "react";
import { Button, ScrollView, Switch, Text, View } from "react-native";
import type { CanvasRef } from "react-native-webgpu";
import { Canvas } from "react-native-webgpu";
import * as THREE from "three";

import {
  disposeWebGPURenderer,
  makeWebGPURenderer,
} from "../ThreeJS/components/makeWebGPURenderer";

import {
  diagnosticStyles,
  drawClearFrame,
  initGPU,
  useDiagnosticLog,
} from "./surfaceLifecycle";

// Android crash when the device is destroyed before the native surface is
// released (SIGSEGV, fault addr 0x20):
//
//   dawn::native::vulkan::FencedDeleter::DeleteWhenUnused
//   dawn::native::vulkan::SwapChain::DetachFromSurfaceImpl
//   dawn::native::Surface::~Surface
//   rnwgpu::SurfaceInfo::detach(bool)
//   Java_com_webgpu_WebGPUView_onViewDestroyed
//
// three's WebGPURenderer.dispose() calls device.destroy() synchronously from
// the React effect cleanup. On Android the native view is only dropped on the
// following frame, so when WebGPUViewManager.onDropViewInstance finally runs
// SurfaceInfo::detach(), it releases a wgpu::Surface whose swapchain is still
// attached. ~Surface runs SwapChain::DetachFromSurfaceImpl, which reaches into
// the already-destroyed device's FencedDeleter: that pointer is gone, so Dawn
// dereferences null at 0x20.
//
// Calling context.unconfigure() before destroy() is not enough (the switch
// below lets you verify this): what touches the device is the swapchain
// detach inside ~Surface, so the surface has to be released (its destructor
// has to run) while the device is still alive.
//
// Steps: press "Unmount canvas" (or navigate back). The inner component's
// cleanup destroys the device, then the native view is dropped one frame
// later. Expected: the log shows the device-lost event and the app survives.
// On a broken build the app dies in native code right after the unmount.
//
// The "raw" mode reproduces the exact call order without three.js; the
// three.js mode runs the reporter's scenario (renderer.dispose() in the
// effect cleanup). Both crash the same way.

type Mode = "raw" | "three";

interface SceneProps {
  mode: Mode;
  opaque: boolean;
  unconfigureFirst: boolean;
  append: (line: string) => void;
}

const RawScene = ({ opaque, unconfigureFirst, append }: SceneProps) => {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    let running = true;
    let frame = 0;
    let device: GPUDevice | null = null;
    let ctx: ReturnType<CanvasRef["getContext"]> = null;
    (async () => {
      const gpu = await initGPU(append);
      if (!running || !ref.current) {
        // Unmounted before the device arrived: destroy it right away so the
        // cleanup below still exercises the same order (destroy, then the
        // view is dropped).
        gpu.device.destroy();
        return;
      }
      ({ device } = gpu);
      ctx = ref.current.getContext("webgpu")!;
      ctx.configure({
        device,
        format: gpu.format,
        alphaMode: "premultiplied",
      });
      append("rendering, now unmount the canvas");
      const loop = () => {
        if (!running || !device || !ctx) {
          return;
        }
        try {
          drawClearFrame(device, ctx, frame++);
          if (frame % 120 === 0) {
            append(`frame ${frame} ok`);
          }
        } catch (e) {
          append(`frame ${frame}: ${e}`);
        }
        requestAnimationFrame(loop);
      };
      loop();
    })();
    return () => {
      running = false;
      if (!device) {
        return;
      }
      // Mirrors WebGPURenderer.dispose(): the device dies synchronously in
      // the effect cleanup, while the native view (and the swapchain attached
      // to its surface) is still alive.
      if (unconfigureFirst && ctx) {
        append("cleanup: context.unconfigure()");
        ctx.unconfigure();
      }
      append("cleanup: device.destroy() (surface still attached)");
      device.destroy();
      append("cleanup done, the native view is dropped next frame");
    };
  }, [append, unconfigureFirst]);

  return <Canvas ref={ref} style={diagnosticStyles.canvas} opaque={opaque} />;
};

const ThreeScene = ({ opaque, append }: SceneProps) => {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    const context = ref.current!.getContext("webgpu")!;
    const { width, height } = context.canvas;

    const camera = new THREE.PerspectiveCamera(70, width / height, 0.01, 10);
    camera.position.z = 1;
    const scene = new THREE.Scene();
    const mesh = new THREE.Mesh(
      new THREE.BoxGeometry(0.2, 0.2, 0.2),
      new THREE.MeshNormalMaterial(),
    );
    scene.add(mesh);

    const renderer = makeWebGPURenderer(context);
    renderer.init().then(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const { device } = renderer.backend as any as {
        device: GPUDevice | undefined;
      };
      device?.lost.then((info) => {
        append(`[device lost] ${info.reason}: ${info.message}`);
      });
      append("three.js rendering, now unmount the canvas");
    });

    let frame = 0;
    renderer.setAnimationLoop((time: number) => {
      mesh.rotation.x = time / 2000;
      mesh.rotation.y = time / 1000;
      renderer.render(scene, camera);
      context.present();
      if (++frame % 120 === 0) {
        append(`frame ${frame} ok`);
      }
    });
    return () => {
      // WebGPURenderer.dispose() -> WebGPUBackend.dispose() ->
      // device.destroy(), synchronously, before the native view is dropped.
      append("cleanup: renderer.dispose() (calls device.destroy())");
      disposeWebGPURenderer(renderer);
      append("cleanup done, the native view is dropped next frame");
    };
  }, [append]);

  return <Canvas ref={ref} style={diagnosticStyles.canvas} opaque={opaque} />;
};

export const DeviceDestroyBeforeDetach = () => {
  const { log, append } = useDiagnosticLog();
  const [mounted, setMounted] = useState(true);
  const [mode, setMode] = useState<Mode>("raw");
  const [opaque, setOpaque] = useState(true);
  const [unconfigureFirst, setUnconfigureFirst] = useState(false);
  const Scene = mode === "raw" ? RawScene : ThreeScene;

  return (
    <View style={diagnosticStyles.container}>
      <View style={diagnosticStyles.controls}>
        <Text style={diagnosticStyles.description}>
          The canvas is configured and rendering. Unmounting it destroys the
          device synchronously in the effect cleanup (like three's
          WebGPURenderer.dispose()); the native view is dropped one frame later
          and detaches a surface whose swapchain still references the dead
          device. Expected: a device-lost log line and no crash. On a broken
          Android build the app dies with SIGSEGV (fault addr 0x20) in
          SwapChain::DetachFromSurfaceImpl.
        </Text>
        <View style={{ flexDirection: "row", gap: 8 }}>
          <Button
            title={mounted ? "Unmount canvas" : "Remount canvas"}
            onPress={() => setMounted((m) => !m)}
          />
          <Button
            title={mode === "raw" ? "Mode: raw WebGPU" : "Mode: three.js"}
            disabled={mounted}
            onPress={() => setMode((m) => (m === "raw" ? "three" : "raw"))}
          />
        </View>
        <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
          <Switch value={opaque} onValueChange={setOpaque} disabled={mounted} />
          <Text style={diagnosticStyles.description}>
            opaque (SurfaceView) / non-opaque (TextureView)
          </Text>
        </View>
        {mode === "raw" ? (
          <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
            <Switch
              value={unconfigureFirst}
              onValueChange={setUnconfigureFirst}
              disabled={mounted}
            />
            <Text style={diagnosticStyles.description}>
              context.unconfigure() before device.destroy() (still crashes)
            </Text>
          </View>
        ) : null}
      </View>
      {mounted ? (
        <Scene
          mode={mode}
          opaque={opaque}
          unconfigureFirst={unconfigureFirst}
          append={append}
        />
      ) : (
        <View style={diagnosticStyles.canvas} />
      )}
      <ScrollView style={diagnosticStyles.log}>
        {log.map((line, i) => (
          <Text key={i} style={diagnosticStyles.logLine}>
            {line}
          </Text>
        ))}
      </ScrollView>
    </View>
  );
};
