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

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";
import { CAMERA_ENV_SHADER } from "./cameraEnvShader";

// Live camera as a three.js environment map. The GLTF helmet renders with
// three.js' WebGPURenderer; its env map is a THREE.ExternalTexture wrapping a
// GPUTexture we own. A Vision Camera frame-processor worklet writes each
// camera frame into that GPUTexture via its own render pass. Three.js and the
// worklet share a single GPUDevice (the one three.js creates internally), so
// the queue ordering between "write env" and "sample env" is automatic.
//
// The front-camera frame is wrapped around the inside of a large back-side
// sphere centered on the helmet, so the CubeCamera at the helmet's middle
// samples camera content in every direction. The helmet's reflections
// therefore fully cover the helmet and never reveal the frame's edges. The
// trade-off is the obvious "face wrapped around the world" look on grazing
// reflections; that's chosen deliberately, since full coverage matters more
// than the seam artifact for this demo.

// 9:16 portrait — front cam delivers 16:9 landscape and the shader rotates it
// 90° to selfie-upright, so this aspect lets the rotated frame fill the env
// texture with no stretching. Cube face dimension matches ENV_HEIGHT. Each
// frame we do (env write + 6 cube faces + optional mipmap chain), so this
// knob drives most of the per-frame GPU cost.
const ENV_WIDTH = 540;
const ENV_HEIGHT = 960;

// Cube face size is mode-dependent. Chrome is a pure mirror reflection, so it
// wants the env's full resolution. PBR samples roughness-selected mips (the
// helmet's metal-roughness keeps most surfaces in the 0.3-0.6 range, i.e.
// mip 2-3), so a 128 cube + its short mip chain is visually indistinguishable
// from 512 while cutting cubemap fill rate ~16x and mip-regen cost with it.
const CUBE_SIZE_PBR = 128;
const CUBE_SIZE_CHROME = 512;

// PBR mode is runtime-toggleable from a button in the JSX below. PBR uses
// the GLTF's MeshStandardMaterial textures (albedo / normal / metalRoughness
// / AO) plus a cubemap envMap for the live reflection; "chrome" mode swaps
// every mesh for a single MeshBasicMaterial that just samples the cubemap.
// Chrome is roughly 5-10x cheaper on fragments and also skips the per-frame
// cubemap mipmap regen the PBR roughness lookup needs.

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
  // Live camera preview, rendered as a native view behind the WebGPU canvas.
  // The worklet still writes the camera into our env texture for the helmet
  // reflection, but the *backdrop* now comes straight from this native
  // preview — no detour through equirect/cubemap, no quality loss.
  const previewOutput = usePreviewOutput();

  // Two cameras at once: the back camera feeds the native preview backdrop,
  // the front camera feeds the helmet's environment map (so you see yourself
  // reflected in the chrome). Requires multi-cam capable hardware (iPhone
  // XS+ / most modern Android flagships).
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

  // PBR is the default. Toggling to chrome swaps every helmet material for
  // a single MeshBasicMaterial that just samples the cubemap. usePBRRef
  // shadows the state so the (one-shot) setup effect can read the *current*
  // value when it first applies materials, even if the user has already
  // toggled before three.js finished initializing.
  const [usePBR, setUsePBR] = useState(true);
  const usePBRRef = useRef(true);
  const applyPBRFnRef = useRef<((pbr: boolean) => void) | null>(null);
  const togglePBR = () => {
    const next = !usePBRRef.current;
    usePBRRef.current = next;
    setUsePBR(next);
    applyPBRFnRef.current?.(next);
  };

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

        // alpha:true configures the canvas with premultiplied alpha mode, so
        // pixels outside the helmet stay transparent and the native camera
        // preview behind the canvas shows through.
        renderer = makeWebGPURenderer(context, { device, alpha: true });
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        renderer.setClearColor(0x000000, 0);
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
        // sampleable 2D texture. Used below as the .map of a billboarded
        // plane that represents the viewer's screen inside the env layer.
        const envExternalTexture = new THREE.ExternalTexture(envTexture);
        envExternalTexture.colorSpace = THREE.SRGBColorSpace;
        (envExternalTexture as unknown as { image: unknown }).image = {
          width: ENV_WIDTH,
          height: ENV_HEIGHT,
        };
        envExternalTexture.needsUpdate = true;

        // The cubemap is rendered each frame by THREE.CubeCamera (six
        // perspective passes into the six faces) instead of being blitted
        // from an equirect. CubeCamera renders the actual scene, so the
        // reflection picks up any other geometry we add — not just the sky
        // sphere that carries the camera feed.
        // Two cube targets, one per mode. We swap which one CubeCamera writes
        // into on toggle (resizing a single target via setSize doesn't fully
        // reallocate cleanly on the WebGPU path, the reflection comes back
        // blurry). Only the active one gets updated each frame so the cost
        // stays paid for one mode at a time.
        const makeCubeRT = (size: number) => {
          const rt = new THREE.CubeRenderTarget(size);
          rt.texture.mapping = THREE.CubeReflectionMapping;
          rt.texture.colorSpace = THREE.SRGBColorSpace;
          // Mipmaps only matter for PBR (roughness-aware sample). Chrome
          // samples mip 0 only, so we skip the regen cost on the 512 target.
          const wantsMips = size <= CUBE_SIZE_PBR;
          rt.texture.generateMipmaps = wantsMips;
          rt.texture.minFilter = wantsMips
            ? THREE.LinearMipmapLinearFilter
            : THREE.LinearFilter;
          rt.texture.magFilter = THREE.LinearFilter;
          return rt;
        };
        const cubeRTPbr = makeCubeRT(CUBE_SIZE_PBR);
        const cubeRTChrome = makeCubeRT(CUBE_SIZE_CHROME);
        const activeCubeRT = () =>
          usePBRRef.current ? cubeRTPbr : cubeRTChrome;

        const scene = new THREE.Scene();
        // No scene.background — the canvas is alpha-cleared and the native
        // camera preview View sits behind it (see JSX below).

        // Layer split: main camera sees layer 0 (helmet only) so the native
        // preview View remains visible everywhere else; CubeCamera sees
        // layer 1 (the reflection screen + gradient backdrop) so the helmet
        // never reflects itself.
        const ENV_LAYER = 1;

        // Live camera frame wrapped around the inside of a large back-side
        // sphere. The CubeCamera at the helmet's center sees the camera in
        // every direction, so reflections fully cover the helmet without
        // any visible frame edges.
        const envSphere = new THREE.Mesh(
          new THREE.SphereGeometry(50, 64, 32),
          new THREE.MeshBasicMaterial({
            map: envExternalTexture,
            side: THREE.BackSide,
            toneMapped: false,
          }),
        );
        envSphere.layers.set(ENV_LAYER);
        scene.add(envSphere);

        const cubeCamera = new THREE.CubeCamera(0.1, 100, activeCubeRT());
        cubeCamera.layers.set(ENV_LAYER);
        scene.add(cubeCamera);

        // PBR path: keep the GLTF's MeshStandardMaterial intact (albedo /
        // normal / metalRoughness / AO from the original textures) and plug
        // our live cubemap into each material's envMap. We capture the
        // originals so the chrome toggle can swap back and forth without
        // losing them.
        const pbrMaterials = new Map<
          THREE.Mesh,
          THREE.Material | THREE.Material[]
        >();
        gltf.scene.traverse((child) => {
          const mesh = child as THREE.Mesh;
          if (!mesh.isMesh) {
            return;
          }
          pbrMaterials.set(mesh, mesh.material);
          const mats = Array.isArray(mesh.material)
            ? mesh.material
            : [mesh.material];
          for (const m of mats) {
            const std = m as THREE.MeshStandardMaterial;
            std.envMap = cubeRTPbr.texture;
            std.envMapIntensity = 1.0;
            std.needsUpdate = true;
          }
        });

        // Chrome path: every mesh shares a single MeshBasicMaterial that
        // just samples the cubemap. No surface detail, but ~5-10x cheaper
        // fragment cost, a useful A/B against PBR on the same scene.
        // MeshBasicMaterial samples the envMap at full intensity (no Fresnel
        // / roughness attenuation like PBR), then ACES tone-maps the result.
        // Bright camera regions land in ACES's compressed range and read as
        // washed-out. Dimming the base color multiplies the env sample down
        // before tone mapping; ~70% matches a real chrome surface's
        // reflectance and brings the brightness back in line with PBR.
        const chromeMaterial = new THREE.MeshBasicMaterial({
          envMap: cubeRTChrome.texture,
          color: 0xb0b0b0,
        });

        const applyPBR = (pbr: boolean) => {
          (
            cubeCamera as unknown as { renderTarget: THREE.CubeRenderTarget }
          ).renderTarget = pbr ? cubeRTPbr : cubeRTChrome;
          for (const [mesh, original] of pbrMaterials) {
            mesh.material = pbr ? original : chromeMaterial;
          }
        };
        applyPBR(usePBRRef.current);
        applyPBRFnRef.current = applyPBR;

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
        camera.position.set(0, 0, 3);

        const clock = new THREE.Clock();
        const distance = 3;
        let frameCount = 0;
        const animate = () => {
          // Slow time-based orbit around the helmet, matching the three.js
          // env-map reference demo.
          const elapsed = clock.getElapsedTime();
          camera.position.x = Math.sin(elapsed * 0.4) * distance;
          camera.position.z = Math.cos(elapsed * 0.4) * distance;
          camera.position.y = 0;
          camera.lookAt(0, 0, 0);

          // Refresh the cubemap by rendering the env-layer (camera sphere)
          // from six perspectives. Costlier than an equirect blit but lets
          // us add other layer-1 props later that would also show up in
          // reflections.
          cubeCamera.update(renderer!, scene);
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
      applyPBRFnRef.current = null;
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
    // 720p front-cam frames are plenty for the helmet's reflection — it's a
    // small on-screen area. Keeping this low matters more in a multi-cam
    // session, where both cameras share AVFoundation's bandwidth budget.
    targetResolution: CommonResolutions.HD_16_9,
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

  // ---- Multi-cam session ------------------------------------------------
  // useCamera always sets enableMultiCamSupport=false, so we drop down to
  // the imperative API to drive two camera connections from a single
  // session: front → frameOutput (helmet env), back → previewOutput
  // (backdrop View).
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

  // Configure the session with two connections once everything is ready.
  // We wait on pipelineState too because the worklet (which receives the
  // front cam frames) only has somewhere to write once the env texture +
  // camera-copy pipeline exist.
  useEffect(() => {
    if (!session || !backDevice || !frontDevice || !pipelineState) {
      return;
    }
    console.log("[CameraHelmet] configuring multi-cam session");
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
        console.log("[CameraHelmet] session configured, starting");
        session.start();
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraHelmet] session configure failed: " + String(e));
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
      <TouchableOpacity
        onPress={togglePBR}
        style={styles.toggleButton}
        activeOpacity={0.8}
      >
        <Text style={styles.toggleButtonText}>{usePBR ? "PBR" : "Chrome"}</Text>
      </TouchableOpacity>
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  // Transparent canvas overlaid on the native camera preview view.
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
  toggleButton: {
    position: "absolute",
    top: 60,
    right: 16,
    backgroundColor: "rgba(0, 0, 0, 0.6)",
    paddingHorizontal: 16,
    paddingVertical: 10,
    borderRadius: 20,
    minWidth: 88,
    alignItems: "center",
  },
  toggleButtonText: { color: "white", fontSize: 14, fontWeight: "600" },
});
