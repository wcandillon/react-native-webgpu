import React, { useEffect, useRef, useState } from "react";
import {
  Linking,
  Platform,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import {
  useCamera,
  useCameraDevices,
  useCameraPermission,
  useFrameOutput,
} from "react-native-vision-camera";
import * as THREE from "three";

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";
import { CAMERA_ENV_SHADER } from "./cameraEnvShader";

// Live camera as a three.js environment map. The GLTF helmet renders with
// three.js' WebGPURenderer; its env map is a THREE.ExternalTexture wrapping a
// GPUTexture we own. A Vision Camera frame-processor worklet writes each
// camera frame into that GPUTexture via its own render pass. Three.js and the
// worklet share a single GPUDevice (the one three.js creates internally), so
// the queue ordering between "write env" and "sample env" is automatic.

// Equirectangular panorama aspect (2:1). Sized larger than the 1280x720
// source so the backdrop reads sharp on retina screens; the equirect→cube
// pass downsamples cleanly. Cube face dimension matches ENV_HEIGHT so the
// cubemap doesn't waste detail.
const ENV_WIDTH = 2048;
const ENV_HEIGHT = 1024;

// Vision Camera + react-native-wgpu both want these features for the external
// texture path. dawn-multi-planar-formats lets Dawn interpret NV12 buffers.
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

export const CameraHelmet = () => {
  const { hasPermission, requestPermission } = useCameraPermission();
  useEffect(() => {
    if (!hasPermission) {
      requestPermission();
    }
  }, [hasPermission, requestPermission]);

  if (!hasPermission) {
    return (
      <View style={styles.permissionContainer}>
        <Text style={styles.permissionText}>
          Camera access is required. Grant it in Settings or tap below.
        </Text>
        <TouchableOpacity
          onPress={() => Linking.openSettings()}
          style={styles.permissionButton}
        >
          <Text style={styles.permissionButtonText}>Open Settings</Text>
        </TouchableOpacity>
      </View>
    );
  }
  return <Scene />;
};

const Scene = () => {
  useEffect(() => {
    console.log("[CameraHelmet] Scene mounted");
    return () => console.log("[CameraHelmet] Scene unmounted");
  }, []);
  const ref = useRef<CanvasRef>(null);
  const gltf = useGLTF(require("./assets/helmet/DamagedHelmet.gltf"));

  const devices = useCameraDevices();
  const cameraDevice = React.useMemo(
    () =>
      devices.find((d) => d.position === "back") ??
      devices.find((d) => d.position === "front") ??
      devices[0],
    [devices],
  );

  const [pipelineState, setPipelineState] = useState<{
    device: GPUDevice;
    cameraPipeline: GPURenderPipeline;
    cameraSampler: GPUSampler;
    envTexture: GPUTexture;
    envTextureView: GPUTextureView;
  } | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);

  // Acquire the GPU device on its own effect. By the time the async adapter +
  // device requests resolve, the Canvas component has been rendered and its
  // ref populated, so the main setup effect (gated on `device`) can grab the
  // GPUCanvasContext synchronously. Same two-effect pattern as
  // VisionCamera.tsx / ChromeSphere.tsx.
  useEffect(() => {
    console.log("[CameraHelmet] device-acquisition effect fired");
    let cancelled = false;
    (async () => {
      try {
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
          throw new Error("requestAdapter returned null");
        }
        const requiredFeatures = [...adapter.features] as GPUFeatureName[];
        const missing = REQUIRED_FEATURES.filter(
          (f) => !adapter.features.has(f),
        );
        const needsAndroidExt =
          Platform.OS === "android" && !adapter.features.has(OPAQUE_YCBCR_EXT);
        if (missing.length > 0 || needsAndroidExt) {
          throw new Error(
            "Adapter doesn't advertise the features the Vision Camera " +
              "external-texture path needs: " +
              `${[...missing, needsAndroidExt ? OPAQUE_YCBCR_EXT : null]
                .filter(Boolean)
                .join(", ")}.`,
          );
        }
        const d = await adapter.requestDevice({ requiredFeatures });
        console.log(
          "[CameraHelmet] device acquired, features: " +
            [...d.features].sort().join(", "),
        );
        if (cancelled) {
          d.destroy();
          return;
        }
        setDevice(d);
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraHelmet] device acquisition failed: " + String(e));
        setError(String(e));
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  // Note: pipelineState is intentionally not in the deps array. Including it
  // would re-run the effect when we call setPipelineState below — React would
  // run the cleanup (which calls setAnimationLoop(null)) and then the effect
  // would bail on the pipelineState guard, leaving us with no render loop.
  // The effect only needs to fire once, when `device` transitions to set.

  useEffect(() => {
    console.log(
      "[CameraHelmet] setup effect fired, device=" +
        String(device != null) +
        " gltf=" +
        String(gltf != null),
    );
    if (!device || !gltf) {
      return;
    }
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      console.log(
        "[CameraHelmet] no webgpu context yet (ref.current=" +
          String(ref.current != null) +
          ") — bailing this effect run",
      );
      return;
    }
    let cancelled = false;
    let renderer: THREE.WebGPURenderer | null = null;

    console.log("[CameraHelmet] context acquired, building three.js scene");
    (async () => {
      try {
        const { width, height } = context.canvas;
        console.log(
          "[CameraHelmet] canvas size = " +
            String(width) +
            "x" +
            String(height),
        );

        renderer = makeWebGPURenderer(context, { device });
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        await renderer.init();
        console.log("[CameraHelmet] three.js renderer init complete");
        if (cancelled) {
          return;
        }

        // Env GPUTexture: render target on our side, sampleable on three's
        // side. rgba8unorm + RENDER_ATTACHMENT|TEXTURE_BINDING lets the
        // single resource pivot between the two roles via implicit barriers.
        const envTexture = device.createTexture({
          size: [ENV_WIDTH, ENV_HEIGHT],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
        });

        // Camera prepass pipeline. Output format matches the env texture so
        // it can be the render target.
        const module = device.createShaderModule({ code: CAMERA_ENV_SHADER });
        const cameraPipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module, entryPoint: "vs_main" },
          fragment: {
            module,
            entryPoint: "fs_main",
            targets: [{ format: "rgba8unorm" }],
          },
          primitive: { topology: "triangle-list" },
        });
        const cameraSampler = device.createSampler({
          magFilter: "linear",
          minFilter: "linear",
        });

        // THREE.ExternalTexture bridges our GPUTexture into three.js as a
        // sampleable 2D equirect. We never set scene.background or
        // material.envMap to this directly: three.js' CubeMapNode would only
        // run the equirect→cubemap conversion once and cache it, which means
        // we'd be stuck sampling whatever was in our env texture on frame 1
        // (= black, before the worklet ever wrote).
        const envExternalTexture = new THREE.ExternalTexture(envTexture);
        envExternalTexture.mapping = THREE.EquirectangularReflectionMapping;
        envExternalTexture.colorSpace = THREE.SRGBColorSpace;
        (envExternalTexture as unknown as { image: unknown }).image = {
          width: ENV_WIDTH,
          height: ENV_HEIGHT,
        };
        envExternalTexture.needsUpdate = true;

        // Allocate the cubemap once. Each frame we'll call
        // cubeRT.fromEquirectangularTexture(renderer, envExternalTexture) to
        // refresh the cube faces from the equirect's *current* contents.
        // That's the same code path CubeMapNode uses internally, but we
        // drive it on every tick instead of letting three.js cache it.
        const cubeRT = new THREE.CubeRenderTarget(ENV_HEIGHT);
        cubeRT.texture.mapping = THREE.CubeReflectionMapping;
        cubeRT.texture.colorSpace = THREE.SRGBColorSpace;

        const scene = new THREE.Scene();
        scene.background = cubeRT.texture;
        // Swap the GLTF helmet's PBR materials for MeshBasicMaterial backed
        // by our cubemap. Same env path that already works for the sphere —
        // no PMREM, no per-frame regeneration headaches — and the helmet's
        // geometry becomes a chrome shell reflecting the camera. Original
        // GLTF textures (albedo, normal, etc.) are dropped on purpose;
        // they'd compete with the reflection for the metal look.
        const chromeMaterial = new THREE.MeshBasicMaterial({
          envMap: cubeRT.texture,
        });
        gltf.scene.traverse((child) => {
          if ((child as THREE.Mesh).isMesh) {
            (child as THREE.Mesh).material = chromeMaterial;
          }
        });
        scene.add(gltf.scene);

        // Drive the perspective from min(width, height) so the helmet keeps
        // a consistent on-screen size in both orientations. three.js'
        // PerspectiveCamera takes a *vertical* FOV; on portrait canvases we
        // derive vFov from a fixed horizontal FOV so the wider dimension
        // never under-frames the helmet.
        const aspect = width / height;
        const baseFov = 45;
        let vFov = baseFov;
        if (aspect < 1) {
          const hFovRad = (baseFov * Math.PI) / 180;
          const vFovRad = 2 * Math.atan(Math.tan(hFovRad / 2) / aspect);
          vFov = (vFovRad * 180) / Math.PI;
        }
        const camera = new THREE.PerspectiveCamera(vFov, aspect, 0.25, 20);
        camera.position.set(0, 0.5, 3);

        const clock = new THREE.Clock();
        let frameCount = 0;
        const animate = () => {
          const elapsed = clock.getElapsedTime();
          const distance = 3;
          camera.position.x = Math.sin(elapsed * 0.4) * distance;
          camera.position.z = Math.cos(elapsed * 0.4) * distance;
          camera.position.y = 0.5;
          camera.lookAt(0, 0, 0);
          // Refresh the cubemap from the (worklet-updated) equirect before
          // rendering the scene. The conversion does 6 fullscreen draws into
          // the cube faces; pipelines are reused across calls.
          cubeRT.fromEquirectangularTexture(renderer!, envExternalTexture);
          renderer!.render(scene, camera);
          context.present();
          frameCount++;
          if (frameCount === 1) {
            console.log("[CameraHelmet] first three.js frame rendered");
          }
        };
        renderer.setAnimationLoop(animate);
        console.log("[CameraHelmet] animation loop started");

        setPipelineState({
          device,
          cameraPipeline,
          cameraSampler,
          envTexture,
          envTextureView: envTexture.createView(),
        });
        console.log("[CameraHelmet] pipelineState set, camera will activate");
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraHelmet] setup failed: " + String(e));
        setError(String(e));
      }
    })();

    return () => {
      console.log("[CameraHelmet] setup-effect cleanup");
      cancelled = true;
      if (renderer) {
        renderer.setAnimationLoop(null);
      }
    };
  }, [device, gltf]);

  // Frame processor worklet: copy the camera frame into envTexture each tick.
  // The single device.queue is shared with three.js, so the helmet pass on
  // the next rAF tick samples this frame's write.
  const logBox = React.useMemo(() => ({ count: 0 }), []);
  const frameOutput = useFrameOutput({
    pixelFormat: "native",
    onFrame: (frame) => {
      "worklet";
      logBox.count += 1;
      if (logBox.count === 1) {
        console.log(
          "[CameraHelmet] worklet first frame, hasPipeline=" +
            String(pipelineState != null) +
            " frame=" +
            String(frame.width) +
            "x" +
            String(frame.height),
        );
      }
      if (!pipelineState) {
        frame.dispose();
        return;
      }
      const {
        device: gpuDevice,
        cameraPipeline,
        cameraSampler,
        envTextureView,
      } = pipelineState;
      const nativeBuffer = frame.getNativeBuffer();
      try {
        const videoFrame = gpuDevice.createVideoFrameFromNativeBuffer(
          nativeBuffer.pointer,
        );
        try {
          const externalTex = gpuDevice.importExternalTexture({
            source: videoFrame,
            label: "camera-helmet-env",
          });
          const bindGroup = gpuDevice.createBindGroup({
            layout: cameraPipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: externalTex },
              { binding: 1, resource: cameraSampler },
            ],
          });
          const encoder = gpuDevice.createCommandEncoder();
          const pass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: envTextureView,
                clearValue: { r: 0, g: 0, b: 0, a: 1 },
                loadOp: "clear",
                storeOp: "store",
              },
            ],
          });
          pass.setPipeline(cameraPipeline);
          pass.setBindGroup(0, bindGroup);
          pass.draw(3);
          pass.end();
          gpuDevice.queue.submit([encoder.finish()]);
        } finally {
          videoFrame.release();
        }
      } finally {
        nativeBuffer.release();
        frame.dispose();
      }
    },
  });

  useCamera({
    isActive: pipelineState != null && cameraDevice != null,
    device: cameraDevice as NonNullable<typeof cameraDevice>,
    outputs: [frameOutput],
  });

  if (error) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>{error}</Text>
      </View>
    );
  }
  if (cameraDevice == null) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>
          No camera available. This screen needs a physical device with a camera
          (the iOS Simulator doesn't have one).
        </Text>
      </View>
    );
  }
  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  canvas: { flex: 1 },
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
  permissionContainer: {
    flex: 1,
    padding: 24,
    justifyContent: "center",
    alignItems: "center",
  },
  permissionText: { fontSize: 16, textAlign: "center", marginBottom: 16 },
  permissionButton: {
    backgroundColor: "#007AFF",
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
  },
  permissionButtonText: { color: "white", fontSize: 16, fontWeight: "600" },
});
