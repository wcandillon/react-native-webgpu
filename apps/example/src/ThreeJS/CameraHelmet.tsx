import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Linking,
  Platform,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import { SensorType, useAnimatedSensor } from "react-native-reanimated";
import { scheduleOnRN } from "react-native-worklets";
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
  CameraOrientation,
  CameraSession,
} from "react-native-vision-camera";
import * as THREE from "three";

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";
import { CAMERA_ENV_SHADER, CAMERA_ENV_UNIFORM_SIZE } from "./cameraEnvShader";

// Live cameras as a three.js environment map, done honestly.
//
// The GLTF helmet renders with three.js' WebGPURenderer over the native back
// camera preview. Its env map is a cubemap that THREE.CubeCamera renders
// from the middle of the helmet. What the CubeCamera sees is built from the
// two cameras of the phone:
//
// - Each camera frame lands in a GPUTexture we own (written by a Vision
//   Camera frame-processor worklet, zero-copy, on the same GPUDevice three.js
//   renders with). Three.js samples it through a THREE.ExternalTexture.
// - Each frame is shown on a flat plane sized from the camera's real field
//   of view (from the intrinsic matrix on iOS, a typical value elsewhere).
//   Seen from the cube camera at the origin, a plane at distance d with width
//   2 * d * tan(fov / 2) is exactly the pinhole image reprojected onto the
//   sphere of directions. The front camera plane sits toward the viewer (that
//   is what a mirror facing you shows in its middle), the back camera plane
//   sits behind the helmet (that is what the rim reflects).
// - The directions neither camera covers are filled once with a heavily
//   blurred copy of both frames on two hemispheres, then the cubemap is no
//   longer cleared: the live planes keep painting into it. Rotating the
//   phone moves the planes across the cube (rotation sensor, world-locked),
//   so panning around the room fills the cube with real content.
// - Frames are decoded to linear light and their highlights are stretched
//   back out so windows and lamps produce real specular hotspots. The env
//   textures and the cube target are half float.

// Portrait env texture. Frames arrive landscape and are rotated upright by
// the copy shader, so 9:16 keeps a 16:9 frame unstretched.
const ENV_WIDTH = 540;
const ENV_HEIGHT = 960;
// Blurred copy used to seed the directions no camera covers.
const BLUR_WIDTH = 36;
const BLUR_HEIGHT = 64;
const BLUR_TAPS = 5;
const BLUR_RADIUS = 0.08;
// Highlight expansion applied in the copy shader (see cameraEnvShader.ts).
const HIGHLIGHT_BOOST = 6;
// Cube target: one half float cubemap with mipmaps, shared by both material
// modes. 256 is enough for the mirror look and keeps the per-frame cube
// render + PMREM refresh affordable.
const CUBE_SIZE = 256;
// Distance of the camera planes from the cube camera. Only the angular
// size matters for the reflection; this just has to sit inside the far
// plane and outside the helmet.
const PLANE_DISTANCE = 10;
const FILLER_RADIUS = 50;
// Frames rendered with the blurred filler visible and the cube cleared,
// before switching to accumulation.
const PRIME_FRAMES = 12;

// Field of view along the frame's long and short axes, as tan(fov / 2).
// Used until the intrinsic matrix reports the real one (iOS only); ~70
// degrees along the long axis is typical for phone cameras in 16:9.
const DEFAULT_LENS = {
  tanHalfLong: Math.tan((70 / 2) * (Math.PI / 180)),
  tanHalfShort: Math.tan((70 / 2) * (Math.PI / 180)) * (9 / 16),
};

const ORIENTATION_CODES: Record<CameraOrientation, number> = {
  up: 0,
  right: 1,
  down: 2,
  left: 3,
};

// Vision Camera + react-native-wgpu both want these features for the external
// texture path. dawn-multi-planar-formats lets Dawn interpret NV12 buffers.
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

type CameraSide = "front" | "back";

interface Lens {
  // Half extents of the upright (portrait) plane per unit of distance.
  tanHalfW: number;
  tanHalfH: number;
  seen: boolean;
}

interface EnvPipeline {
  device: GPUDevice;
  pipeline: GPURenderPipeline;
  sampler: GPUSampler;
  frontSharpView: GPUTextureView;
  frontBlurView: GPUTextureView;
  frontSharpUniforms: GPUBuffer;
  frontBlurUniforms: GPUBuffer;
  backSharpView: GPUTextureView;
  backBlurView: GPUTextureView;
  backSharpUniforms: GPUBuffer;
  backBlurUniforms: GPUBuffer;
}

const encodeUniforms = (
  orientation: number,
  mirrored: boolean,
  taps: number,
  radius: number,
) => {
  "worklet";
  const data = new ArrayBuffer(CAMERA_ENV_UNIFORM_SIZE);
  const u32 = new Uint32Array(data);
  const f32 = new Float32Array(data);
  u32[0] = orientation;
  u32[1] = mirrored ? 1 : 0;
  u32[2] = taps;
  u32[3] = 0;
  f32[4] = HIGHLIGHT_BOOST;
  f32[5] = radius;
  f32[6] = 0;
  f32[7] = 0;
  return data;
};

// Copies one camera frame into its sharp and blurred env textures. Runs on
// the Vision Camera worklet runtime; every GPU object crosses via the
// react-native-wgpu worklet serializer.
const drawCameraFrame = (
  device: GPUDevice,
  pipeline: GPURenderPipeline,
  sampler: GPUSampler,
  videoFrame: ReturnType<GPUDevice["createVideoFrameFromNativeBuffer"]>,
  orientation: number,
  mirrored: boolean,
  sharpView: GPUTextureView,
  sharpUniforms: GPUBuffer,
  blurView: GPUTextureView,
  blurUniforms: GPUBuffer,
) => {
  "worklet";
  const externalTex = device.importExternalTexture({
    source: videoFrame,
    label: "camera-helmet-env",
  });
  device.queue.writeBuffer(
    sharpUniforms,
    0,
    encodeUniforms(orientation, mirrored, 1, 0),
  );
  device.queue.writeBuffer(
    blurUniforms,
    0,
    encodeUniforms(orientation, mirrored, BLUR_TAPS, BLUR_RADIUS),
  );
  const encoder = device.createCommandEncoder();
  const targets: [GPUTextureView, GPUBuffer][] = [
    [sharpView, sharpUniforms],
    [blurView, blurUniforms],
  ];
  for (const [view, uniforms] of targets) {
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: externalTex },
        { binding: 1, resource: sampler },
        { binding: 2, resource: { buffer: uniforms } },
      ],
    });
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          view,
          clearValue: { r: 0, g: 0, b: 0, a: 1 },
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.draw(3);
    pass.end();
  }
  device.queue.submit([encoder.finish()]);
};

const setLayerDeep = (object: THREE.Object3D, layer: number) => {
  object.traverse((child) => child.layers.set(layer));
};

// Mirrors the U axis of a geometry's uv attribute. The hemispheres carry the
// blurred frames on their inside; seen from the center, SphereGeometry's uv
// runs right-to-left, so flip it to keep left and right where the planes
// have them.
const flipU = (geometry: THREE.BufferGeometry) => {
  const uv = geometry.attributes.uv as THREE.BufferAttribute;
  for (let i = 0; i < uv.count; i++) {
    uv.setX(i, 1 - uv.getX(i));
  }
  uv.needsUpdate = true;
  return geometry;
};

// Half extents of the upright plane per unit distance, from the frame's
// intrinsic matrix when available (fx, fy in pixels along the frame's own
// axes) or the default lens otherwise. A 90 degree orientation swaps which
// frame axis ends up vertical.
const lensFromFrame = (
  width: number,
  height: number,
  intrinsics: number[] | undefined,
  orientation: number,
) => {
  "worklet";
  let tanHalfX = DEFAULT_LENS.tanHalfLong;
  let tanHalfY = DEFAULT_LENS.tanHalfShort;
  if (intrinsics != null && intrinsics.length >= 5) {
    const [fx, , , , fy] = intrinsics;
    if (fx > 0 && fy > 0) {
      tanHalfX = width / (2 * fx);
      tanHalfY = height / (2 * fy);
    }
  }
  const rotated = orientation === 1 || orientation === 3;
  return rotated
    ? { tanHalfW: tanHalfY, tanHalfH: tanHalfX }
    : { tanHalfW: tanHalfX, tanHalfH: tanHalfY };
};

const DEFAULT_LENS_PORTRAIT = () => ({
  tanHalfW: DEFAULT_LENS.tanHalfShort,
  tanHalfH: DEFAULT_LENS.tanHalfLong,
});

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
  // Live back camera preview, rendered as a native view behind the WebGPU
  // canvas. The same connection also feeds a frame output for the env map.
  const previewOutput = usePreviewOutput();

  // Two cameras at once: the back camera feeds the backdrop and the far
  // half of the environment, the front camera feeds the near half (you).
  // Requires multi-cam capable hardware (iPhone XS+ / most modern Android
  // flagships).
  const devices = useCameraDevices();
  const backDevice = React.useMemo(
    () => devices.find((d) => d.position === "back"),
    [devices],
  );
  const frontDevice = React.useMemo(
    () => devices.find((d) => d.position === "front"),
    [devices],
  );

  // Device attitude, used to keep the accumulated environment world-locked
  // while the phone turns. Falls back to a device-locked env when the
  // sensor is unavailable (simulator, some Android devices).
  const rotation = useAnimatedSensor(SensorType.ROTATION, { interval: 16 });

  const [pipelineState, setPipelineState] = useState<EnvPipeline | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [device, setDevice] = useState<GPUDevice | null>(null);

  // Per-camera lens info reported back from the frame worklets. Read every
  // frame by the render loop to size the env planes.
  const lensRef = useRef<Record<CameraSide, Lens>>({
    front: { ...DEFAULT_LENS_PORTRAIT(), seen: false },
    back: { ...DEFAULT_LENS_PORTRAIT(), seen: false },
  });
  const onLens = useCallback(
    (
      side: CameraSide,
      tanHalfW: number,
      tanHalfH: number,
      orientation: string,
      mirrored: boolean,
      hasIntrinsics: boolean,
    ) => {
      const lens = lensRef.current[side];
      if (!lens.seen) {
        console.log(
          `[CameraHelmet] ${side} camera: orientation=${orientation} ` +
            `mirrored=${String(mirrored)} intrinsics=${String(hasIntrinsics)} ` +
            `hfov=${((2 * Math.atan(tanHalfW) * 180) / Math.PI).toFixed(1)} ` +
            `vfov=${((2 * Math.atan(tanHalfH) * 180) / Math.PI).toFixed(1)}`,
        );
      }
      lens.tanHalfW = tanHalfW;
      lens.tanHalfH = tanHalfH;
      lens.seen = true;
    },
    [],
  );

  // Env accumulation state shared with the render loop. Reset re-primes the
  // cube with the blurred filler, useful once the room has changed.
  const envRef = useRef({ primeFramesLeft: PRIME_FRAMES });
  const resetEnv = () => {
    envRef.current.primeFramesLeft = PRIME_FRAMES;
  };

  // PBR is the default. Chrome swaps every helmet material for a single
  // polished-metal MeshStandardMaterial so the same cubemap, Fresnel and
  // tone mapping apply, only without the helmet's surface detail. usePBRRef
  // shadows the state so the one-shot setup effect can read the current
  // value even if the user toggled before three.js finished initializing.
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
  // GPUCanvasContext synchronously.
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
        console.warn("[CameraHelmet] device acquisition failed: " + String(e));
        setError(String(e));
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  // Note: pipelineState is intentionally not in the deps array. Including it
  // would re-run the effect when we call setPipelineState below: React would
  // run the cleanup (which calls setAnimationLoop(null)) and then the effect
  // would bail on the pipelineState guard, leaving us with no render loop.
  // The effect only needs to fire once, when `device` transitions to set.
  useEffect(() => {
    if (!device || !gltf) {
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

        // alpha:true configures the canvas with premultiplied alpha mode, so
        // pixels outside the helmet stay transparent and the native camera
        // preview behind the canvas shows through.
        renderer = makeWebGPURenderer(context, { device, alpha: true });
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        renderer.setClearColor(0x000000, 0);
        await renderer.init();
        if (cancelled) {
          return;
        }

        // Env GPUTextures: render targets on our side, sampleable on three's
        // side. Half float so the expanded highlights survive.
        const makeEnvTexture = (w: number, h: number, label: string) =>
          device.createTexture({
            label,
            size: [w, h],
            format: "rgba16float",
            usage:
              GPUTextureUsage.RENDER_ATTACHMENT |
              GPUTextureUsage.TEXTURE_BINDING,
          });
        const frontSharp = makeEnvTexture(ENV_WIDTH, ENV_HEIGHT, "env-front");
        const backSharp = makeEnvTexture(ENV_WIDTH, ENV_HEIGHT, "env-back");
        const frontBlur = makeEnvTexture(
          BLUR_WIDTH,
          BLUR_HEIGHT,
          "env-front-blur",
        );
        const backBlur = makeEnvTexture(
          BLUR_WIDTH,
          BLUR_HEIGHT,
          "env-back-blur",
        );
        const makeUniforms = (label: string) =>
          device.createBuffer({
            label,
            size: CAMERA_ENV_UNIFORM_SIZE,
            usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
          });

        // Camera copy pipeline. Output format matches the env textures.
        const module = device.createShaderModule({ code: CAMERA_ENV_SHADER });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module, entryPoint: "vs_main" },
          fragment: {
            module,
            entryPoint: "fs_main",
            targets: [{ format: "rgba16float" }],
          },
          primitive: { topology: "triangle-list" },
        });
        const sampler = device.createSampler({
          magFilter: "linear",
          minFilter: "linear",
        });

        // THREE.ExternalTexture bridges a GPUTexture into three.js as a
        // sampleable 2D texture. Contents are linear light already.
        const wrap = (tex: GPUTexture, w: number, h: number) => {
          const t = new THREE.ExternalTexture(tex);
          t.colorSpace = THREE.LinearSRGBColorSpace;
          t.magFilter = THREE.LinearFilter;
          t.minFilter = THREE.LinearFilter;
          (t as unknown as { image: unknown }).image = { width: w, height: h };
          t.needsUpdate = true;
          return t;
        };
        const frontSharpTex = wrap(frontSharp, ENV_WIDTH, ENV_HEIGHT);
        const backSharpTex = wrap(backSharp, ENV_WIDTH, ENV_HEIGHT);
        const frontBlurTex = wrap(frontBlur, BLUR_WIDTH, BLUR_HEIGHT);
        const backBlurTex = wrap(backBlur, BLUR_WIDTH, BLUR_HEIGHT);

        // One half float cubemap, rendered by THREE.CubeCamera from the
        // env layer. Mipmaps feed the roughness lookup of the PBR materials.
        const cubeRT = new THREE.CubeRenderTarget(CUBE_SIZE, {
          type: THREE.HalfFloatType,
          generateMipmaps: true,
          minFilter: THREE.LinearMipmapLinearFilter,
          magFilter: THREE.LinearFilter,
        });
        cubeRT.texture.mapping = THREE.CubeReflectionMapping;

        const scene = new THREE.Scene();
        // No scene.background: the canvas is alpha-cleared and the native
        // camera preview View sits behind it (see JSX below).

        // Layer split: the main camera sees layer 0 (helmet only) so the
        // native preview View remains visible everywhere else; the
        // CubeCamera sees layer 1 (camera planes + filler) so the helmet
        // never reflects itself.
        const ENV_LAYER = 1;

        // Everything attached to the phone lives under `live`: the camera
        // planes and the filler hemispheres. The helmet and the main camera
        // live under `rig`. Both groups follow the device attitude, while
        // the CubeCamera stays world-aligned at the origin, so the cubemap
        // accumulates in world space.
        const live = new THREE.Group();
        const rig = new THREE.Group();
        scene.add(live, rig);

        const planeMaterial = (map: THREE.Texture) =>
          new THREE.MeshBasicMaterial({
            map,
            side: THREE.DoubleSide,
            toneMapped: false,
            depthTest: false,
            depthWrite: false,
          });
        // Unit planes, scaled every frame from the reported field of view.
        // lookAt turns the plane's front toward the cube camera.
        const frontPlane = new THREE.Mesh(
          new THREE.PlaneGeometry(1, 1),
          planeMaterial(frontSharpTex),
        );
        frontPlane.position.set(0, 0, PLANE_DISTANCE);
        frontPlane.lookAt(0, 0, 0);
        frontPlane.renderOrder = 1;
        const backPlane = new THREE.Mesh(
          new THREE.PlaneGeometry(1, 1),
          planeMaterial(backSharpTex),
        );
        backPlane.position.set(0, 0, -PLANE_DISTANCE);
        backPlane.lookAt(0, 0, 0);
        backPlane.renderOrder = 1;

        // Blurred filler: the front frame on the hemisphere toward the
        // viewer, the back frame on the one behind. Only visible while
        // priming the cube, then the accumulated content takes over.
        const hemisphere = (phiStart: number, map: THREE.Texture) => {
          const mesh = new THREE.Mesh(
            flipU(
              new THREE.SphereGeometry(
                FILLER_RADIUS,
                32,
                16,
                phiStart,
                Math.PI,
              ),
            ),
            new THREE.MeshBasicMaterial({
              map,
              side: THREE.BackSide,
              toneMapped: false,
              depthTest: false,
              depthWrite: false,
            }),
          );
          mesh.renderOrder = 0;
          return mesh;
        };
        const fillerFront = hemisphere(0, frontBlurTex);
        const fillerBack = hemisphere(Math.PI, backBlurTex);
        live.add(frontPlane, backPlane, fillerFront, fillerBack);
        setLayerDeep(live, ENV_LAYER);

        const cubeCamera = new THREE.CubeCamera(0.1, 100, cubeRT);
        cubeCamera.layers.set(ENV_LAYER);
        scene.add(cubeCamera);

        // PBR path: keep the GLTF's MeshStandardMaterial intact (albedo /
        // normal / metalRoughness / AO from the original textures) and plug
        // the live cubemap into each material's envMap.
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
            std.envMap = cubeRT.texture;
            std.envMapIntensity = 1.0;
            std.needsUpdate = true;
          }
        });

        // Chrome path: polished metal. Unlike a basic material this keeps
        // the Fresnel falloff, so the rim reflects more than the center.
        const chromeMaterial = new THREE.MeshStandardMaterial({
          color: 0xffffff,
          metalness: 1,
          roughness: 0.05,
          envMap: cubeRT.texture,
        });

        const applyPBR = (pbr: boolean) => {
          for (const [mesh, original] of pbrMaterials) {
            mesh.material = pbr ? original : chromeMaterial;
          }
        };
        applyPBR(usePBRRef.current);
        applyPBRFnRef.current = applyPBR;

        rig.add(gltf.scene);

        // Drive the perspective from min(width, height) so the helmet keeps
        // a consistent on-screen size in both orientations.
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
        camera.lookAt(0, 0, 0);
        rig.add(camera);

        const clock = new THREE.Clock();
        const attitude = new THREE.Quaternion();
        const startInverse = new THREE.Quaternion();
        const relative = new THREE.Quaternion();
        let haveStart = false;
        let frameCount = 0;
        const animate = () => {
          const dt = clock.getDelta();

          // World-lock: rotate the phone-attached groups by the attitude
          // change since the first reading, so the cube camera (which
          // stays put) sees the live planes sweep across the cube.
          if (rotation.isAvailable) {
            const { qx, qy, qz, qw } = rotation.sensor.value;
            if (qw !== 0 || qx !== 0 || qy !== 0 || qz !== 0) {
              attitude.set(qx, qy, qz, qw);
              if (!haveStart) {
                startInverse.copy(attitude).invert();
                haveStart = true;
              }
              relative.copy(startInverse).multiply(attitude);
              rig.quaternion.copy(relative);
              live.quaternion.copy(relative);
            }
          }
          // Slow spin so the reflection travels across the surface.
          gltf.scene.rotation.y += dt * 0.25;

          const lens = lensRef.current;
          frontPlane.scale.set(
            2 * PLANE_DISTANCE * lens.front.tanHalfW,
            2 * PLANE_DISTANCE * lens.front.tanHalfH,
            1,
          );
          backPlane.scale.set(
            2 * PLANE_DISTANCE * lens.back.tanHalfW,
            2 * PLANE_DISTANCE * lens.back.tanHalfH,
            1,
          );

          // Prime with the blurred filler until both cameras have delivered
          // and a few frames have been rendered, then stop clearing the
          // cube so the live planes accumulate into it.
          const ready = lens.front.seen && lens.back.seen;
          const env = envRef.current;
          const priming = !ready || env.primeFramesLeft > 0;
          if (ready && env.primeFramesLeft > 0) {
            env.primeFramesLeft--;
          }
          fillerFront.visible = priming;
          fillerBack.visible = priming;
          renderer!.autoClearColor = priming;
          cubeCamera.update(renderer!, scene);
          renderer!.autoClearColor = true;

          renderer!.render(scene, camera);
          context.present();
          frameCount++;
          if (frameCount === 1) {
            console.log("[CameraHelmet] first three.js frame rendered");
          }
        };
        renderer.setAnimationLoop(animate);

        setPipelineState({
          device,
          pipeline,
          sampler,
          frontSharpView: frontSharp.createView(),
          frontBlurView: frontBlur.createView(),
          frontSharpUniforms: makeUniforms("env-front-uniforms"),
          frontBlurUniforms: makeUniforms("env-front-blur-uniforms"),
          backSharpView: backSharp.createView(),
          backBlurView: backBlur.createView(),
          backSharpUniforms: makeUniforms("env-back-uniforms"),
          backBlurUniforms: makeUniforms("env-back-blur-uniforms"),
        });
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[CameraHelmet] setup failed: " + String(e));
        setError(String(e));
      }
    })();

    return () => {
      cancelled = true;
      applyPBRFnRef.current = null;
      if (renderer) {
        renderer.setAnimationLoop(null);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [device, gltf]);

  // Frame worklets: copy each camera frame into its env textures. The single
  // device.queue is shared with three.js, so the helmet pass on the next
  // rAF tick samples this frame's write. The first frame (and every 120th,
  // since worklet-side state resets whenever the closure is recreated)
  // reports the lens back to JS so the env planes get their real size.
  const frontCounter = React.useMemo(() => ({ count: 0 }), []);
  const frontFrameOutput = useFrameOutput({
    pixelFormat: "native",
    // 720p is plenty for a reflection. Keeping both streams small matters
    // in a multi-cam session, where the cameras share the ISP's budget.
    targetResolution: CommonResolutions.HD_16_9,
    enableCameraMatrixDelivery: Platform.OS === "ios",
    onFrame: (frame) => {
      "worklet";
      if (!pipelineState) {
        frame.dispose();
        return;
      }
      const orientation = ORIENTATION_CODES[frame.orientation] ?? 1;
      const mirrored = frame.isMirrored;
      if (frontCounter.count % 120 === 0) {
        const lens = lensFromFrame(
          frame.width,
          frame.height,
          frame.cameraIntrinsicMatrix,
          orientation,
        );
        scheduleOnRN(
          onLens,
          "front",
          lens.tanHalfW,
          lens.tanHalfH,
          frame.orientation,
          mirrored,
          frame.cameraIntrinsicMatrix != null,
        );
      }
      frontCounter.count++;
      const nativeBuffer = frame.getNativeBuffer();
      try {
        const videoFrame =
          pipelineState.device.createVideoFrameFromNativeBuffer(
            nativeBuffer.pointer,
          );
        try {
          drawCameraFrame(
            pipelineState.device,
            pipelineState.pipeline,
            pipelineState.sampler,
            videoFrame,
            orientation,
            mirrored,
            pipelineState.frontSharpView,
            pipelineState.frontSharpUniforms,
            pipelineState.frontBlurView,
            pipelineState.frontBlurUniforms,
          );
        } finally {
          videoFrame.release();
        }
      } finally {
        nativeBuffer.release();
        frame.dispose();
      }
    },
  });

  const backCounter = React.useMemo(() => ({ count: 0 }), []);
  const backFrameOutput = useFrameOutput({
    pixelFormat: "native",
    targetResolution: CommonResolutions.HD_16_9,
    enableCameraMatrixDelivery: Platform.OS === "ios",
    onFrame: (frame) => {
      "worklet";
      if (!pipelineState) {
        frame.dispose();
        return;
      }
      const orientation = ORIENTATION_CODES[frame.orientation] ?? 1;
      const mirrored = frame.isMirrored;
      if (backCounter.count % 120 === 0) {
        const lens = lensFromFrame(
          frame.width,
          frame.height,
          frame.cameraIntrinsicMatrix,
          orientation,
        );
        scheduleOnRN(
          onLens,
          "back",
          lens.tanHalfW,
          lens.tanHalfH,
          frame.orientation,
          mirrored,
          frame.cameraIntrinsicMatrix != null,
        );
      }
      backCounter.count++;
      const nativeBuffer = frame.getNativeBuffer();
      try {
        const videoFrame =
          pipelineState.device.createVideoFrameFromNativeBuffer(
            nativeBuffer.pointer,
          );
        try {
          drawCameraFrame(
            pipelineState.device,
            pipelineState.pipeline,
            pipelineState.sampler,
            videoFrame,
            orientation,
            mirrored,
            pipelineState.backSharpView,
            pipelineState.backSharpUniforms,
            pipelineState.backBlurView,
            pipelineState.backBlurUniforms,
          );
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
  // session: back -> preview + env frames, front -> env frames.
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
  // We wait on pipelineState too because the worklets only have somewhere
  // to write once the env textures + copy pipeline exist. Stabilization is
  // off on both connections: intrinsic matrix delivery requires it, and a
  // stabilized crop would change the field of view anyway.
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
              outputs: [
                { output: previewOutput, mirrorMode: "auto" },
                { output: backFrameOutput, mirrorMode: "auto" },
              ],
              constraints: [{ videoStabilizationMode: "off" }],
            },
            {
              input: frontDevice,
              outputs: [{ output: frontFrameOutput, mirrorMode: "auto" }],
              constraints: [{ videoStabilizationMode: "off" }],
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
    frontFrameOutput,
    backFrameOutput,
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
      <View style={styles.toolbar}>
        <TouchableOpacity
          onPress={togglePBR}
          style={styles.toggleButton}
          activeOpacity={0.8}
        >
          <Text style={styles.toggleButtonText}>
            {usePBR ? "PBR" : "Chrome"}
          </Text>
        </TouchableOpacity>
        <TouchableOpacity
          onPress={resetEnv}
          style={styles.toggleButton}
          activeOpacity={0.8}
        >
          <Text style={styles.toggleButtonText}>Reset env</Text>
        </TouchableOpacity>
      </View>
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
  toolbar: {
    position: "absolute",
    top: 60,
    right: 16,
    gap: 8,
    alignItems: "flex-end",
  },
  toggleButton: {
    backgroundColor: "rgba(0, 0, 0, 0.6)",
    paddingHorizontal: 16,
    paddingVertical: 10,
    borderRadius: 20,
    minWidth: 88,
    alignItems: "center",
  },
  toggleButtonText: { color: "white", fontSize: 14, fontWeight: "600" },
});
