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
  CommonResolutions,
  NativePreviewView,
  useCameraDevices,
  useCameraPermission,
  useFrameOutput,
  usePreviewOutput,
  VisionCamera as VisionCameraFactory,
} from "react-native-vision-camera";
import type {
  CameraController,
  CameraSession,
} from "react-native-vision-camera";
import * as THREE from "three";

import { makeWebGPURenderer } from "./components/makeWebGPURenderer";
import { CAMERA_ENV_SHADER } from "./cameraEnvShader";

// Sibling of CameraHelmet but with three procedural chrome spheres in place
// of the GLTF helmet. Multi-cam setup: back camera renders behind the canvas
// as a native preview view, front camera feeds the cubemap that the spheres
// reflect. No PBR (no GLTF assets), so we skip mipmap generation entirely —
// the chrome look stays sharp on all surfaces.

const ENV_WIDTH = 1024;
const ENV_HEIGHT = 512;

const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

// Three big chrome spheres swirling around the origin, inspired by three.js'
// stereo-effects demo (which uses InstancedMesh + per-frame matrix updates).
// Even at 3 instances the InstancedMesh path is still nice because all
// three render in a single draw call.
const BEAD_COUNT = 3;
const BEAD_RADIUS = 0.55;
// XY radius of the swirl. Spheres orbit evenly spaced around the origin.
const SWIRL_RADIUS = 1.8;

export const CameraSpheres = () => {
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
    console.log("[CameraSpheres] Scene mounted");
    return () => console.log("[CameraSpheres] Scene unmounted");
  }, []);
  const ref = useRef<CanvasRef>(null);
  const previewOutput = usePreviewOutput();

  const devices = useCameraDevices();
  const backDevice = React.useMemo(
    () => devices.find((d) => d.position === "back"),
    [devices],
  );
  const frontDevice = React.useMemo(
    () => devices.find((d) => d.position === "front"),
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

  useEffect(() => {
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
        if (cancelled) {
          d.destroy();
          return;
        }
        setDevice(d);
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraSpheres] device acquisition failed: " + String(e));
        setError(String(e));
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (!device) {
      return;
    }
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    let cancelled = false;
    let renderer: THREE.WebGPURenderer | null = null;

    (async () => {
      try {
        const { width, height } = context.canvas;

        renderer = makeWebGPURenderer(context, { device, alpha: true });
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        renderer.setClearColor(0x000000, 0);
        await renderer.init();
        if (cancelled) {
          return;
        }

        const envTexture = device.createTexture({
          size: [ENV_WIDTH, ENV_HEIGHT],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
        });

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

        const envExternalTexture = new THREE.ExternalTexture(envTexture);
        envExternalTexture.mapping = THREE.EquirectangularReflectionMapping;
        envExternalTexture.colorSpace = THREE.SRGBColorSpace;
        (envExternalTexture as unknown as { image: unknown }).image = {
          width: ENV_WIDTH,
          height: ENV_HEIGHT,
        };
        envExternalTexture.needsUpdate = true;

        // Cube target refreshed per frame from the equirect — same dynamic
        // env trick as CameraHelmet. No mipmap chain: the spheres use
        // MeshBasicMaterial which samples mip 0 unconditionally, so the
        // auto-mipmap regeneration would be pure waste.
        const cubeRT = new THREE.CubeRenderTarget(ENV_HEIGHT);
        cubeRT.texture.mapping = THREE.CubeReflectionMapping;
        cubeRT.texture.colorSpace = THREE.SRGBColorSpace;

        const scene = new THREE.Scene();
        // Single InstancedMesh: all three spheres in one draw call. Mesh
        // tessellation matters more here than in the swarm version because
        // each sphere is bigger on screen, so 48x32 segments instead of
        // 16x8 — smoother silhouettes against the panorama backdrop.
        const chrome = new THREE.MeshBasicMaterial({ envMap: cubeRT.texture });
        const geometry = new THREE.SphereGeometry(BEAD_RADIUS, 48, 32);
        const beads = new THREE.InstancedMesh(geometry, chrome, BEAD_COUNT);
        beads.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
        scene.add(beads);

        // Seed each instance's matrix to identity. The animate loop
        // overwrites the translation column each frame; scale and rotation
        // stay as identity (= sphere radius set by BEAD_RADIUS above).
        const dummy = new THREE.Object3D();
        for (let i = 0; i < BEAD_COUNT; i++) {
          dummy.updateMatrix();
          beads.setMatrixAt(i, dummy.matrix);
        }
        beads.instanceMatrix.needsUpdate = true;
        const beadPos = new THREE.Vector3();

        const aspect = width / height;
        const baseFov = 60;
        let vFov = baseFov;
        if (aspect < 1) {
          const hFovRad = (baseFov * Math.PI) / 180;
          const vFovRad = 2 * Math.atan(Math.tan(hFovRad / 2) / aspect);
          vFov = (vFovRad * 180) / Math.PI;
        }
        const camera = new THREE.PerspectiveCamera(vFov, aspect, 0.25, 20);
        camera.position.set(0, 0, 4);

        const clock = new THREE.Clock();
        const animate = () => {
          // Slow rotation of the three spheres around the origin in the XY
          // plane. Phase offsets are evenly spaced (2π/3 apart) so the
          // spheres form a rotating equilateral triangle, never overlapping.
          const elapsed = clock.getElapsedTime() * 0.4;
          for (let i = 0; i < BEAD_COUNT; i++) {
            const angle = elapsed + (i * 2 * Math.PI) / BEAD_COUNT;
            beadPos.set(
              SWIRL_RADIUS * Math.cos(angle),
              SWIRL_RADIUS * Math.sin(angle),
              0,
            );
            beads.getMatrixAt(i, dummy.matrix);
            dummy.matrix.setPosition(beadPos);
            beads.setMatrixAt(i, dummy.matrix);
          }
          beads.instanceMatrix.needsUpdate = true;

          cubeRT.fromEquirectangularTexture(renderer!, envExternalTexture);
          renderer!.render(scene, camera);
          context.present();
        };
        renderer.setAnimationLoop(animate);

        setPipelineState({
          device,
          cameraPipeline,
          cameraSampler,
          envTexture,
          envTextureView: envTexture.createView(),
        });
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraSpheres] setup failed: " + String(e));
        setError(String(e));
      }
    })();

    return () => {
      cancelled = true;
      if (renderer) {
        renderer.setAnimationLoop(null);
      }
    };
  }, [device]);

  const logBox = React.useMemo(() => ({ count: 0 }), []);
  const frameOutput = useFrameOutput({
    pixelFormat: "native",
    targetResolution: CommonResolutions.HD_16_9,
    onFrame: (frame) => {
      "worklet";
      logBox.count += 1;
      if (logBox.count === 1) {
        console.log(
          "[CameraSpheres] worklet first frame, frame=" +
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
            label: "camera-spheres-env",
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

  const [session, setSession] = useState<CameraSession | null>(null);
  useEffect(() => {
    if (!VisionCameraFactory.supportsMultiCamSessions) {
      setError(
        "This device doesn't support multi-cam sessions. Need an iPhone XS " +
          "or newer / a comparable Android flagship.",
      );
      return;
    }
    let cancelled = false;
    let created: CameraSession | null = null;
    (async () => {
      const s = await VisionCameraFactory.createCameraSession(true);
      if (cancelled) {
        s.dispose();
        return;
      }
      created = s;
      setSession(s);
    })();
    return () => {
      cancelled = true;
      created?.stop();
      created?.dispose();
    };
  }, []);

  useEffect(() => {
    if (!session || !backDevice || !frontDevice || !pipelineState) {
      return;
    }
    let cancelled = false;
    let controllers: CameraController[] = [];
    (async () => {
      try {
        controllers = await session.configure(
          [
            {
              input: backDevice,
              outputs: [{ output: previewOutput, mirrorMode: "auto" }],
              constraints: [],
            },
            {
              input: frontDevice,
              outputs: [{ output: frameOutput, mirrorMode: "auto" }],
              constraints: [],
            },
          ],
          {},
        );
        if (cancelled) {
          controllers.forEach((c) => c.dispose());
          return;
        }
        session.start();
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraSpheres] session configure failed: " + String(e));
        setError(String(e));
      }
    })();
    return () => {
      cancelled = true;
      session.stop();
      controllers.forEach((c) => c.dispose());
    };
  }, [
    session,
    backDevice,
    frontDevice,
    previewOutput,
    frameOutput,
    pipelineState,
  ]);

  if (error) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>{error}</Text>
      </View>
    );
  }
  if (backDevice == null || frontDevice == null) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>
          Need both a back and a front camera. The iOS Simulator has none, and
          some devices expose only one.
        </Text>
      </View>
    );
  }
  return (
    <View style={styles.root}>
      <NativePreviewView
        previewOutput={previewOutput}
        style={StyleSheet.absoluteFill}
      />
      <Canvas ref={ref} style={styles.canvas} transparent />
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  canvas: { ...StyleSheet.absoluteFillObject, backgroundColor: "transparent" },
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
