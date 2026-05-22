/* eslint-disable prefer-destructuring */
import React, { useEffect } from "react";
import {
  Linking,
  PixelRatio,
  Platform,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from "react-native";
import {
  Canvas,
  useCanvasRef,
  type NativeCanvas,
  type RNCanvasContext,
} from "react-native-wgpu";
import {
  useCamera,
  useCameraDevices,
  useCameraPermission,
  useFrameOutput,
} from "react-native-vision-camera";

import { BLUR_SHADER, PREPASS_SHADER } from "../VisionCamera/blurShaders";

import {
  generateSphere,
  NORMAL_OFFSET,
  POSITION_OFFSET,
  VERTEX_STRIDE_BYTES,
} from "./geometry";
import { BACKDROP_SHADER, SHADER } from "./shader";

// All matrix math runs inside the Vision Camera frame-processor worklet, so it
// has to be implemented with worklet-friendly helpers. wgpu-matrix calls
// `mat4.create` / `vec3.create` internally and those aren't marked as
// worklets, so we inline what we need here. Conventions match wgpu-matrix:
// column-major 4x4 stored as Float32Array(16), index = col * 4 + row,
// right-handed view space, perspective maps z to [0, 1] (WebGPU clip space).

const setIdentity = (out: Float32Array) => {
  "worklet";
  out[0] = 1;
  out[1] = 0;
  out[2] = 0;
  out[3] = 0;
  out[4] = 0;
  out[5] = 1;
  out[6] = 0;
  out[7] = 0;
  out[8] = 0;
  out[9] = 0;
  out[10] = 1;
  out[11] = 0;
  out[12] = 0;
  out[13] = 0;
  out[14] = 0;
  out[15] = 1;
};

// dst = m * Ry(angle). Safe with dst === m.
const applyRotateY = (m: Float32Array, angle: number, dst: Float32Array) => {
  "worklet";
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  const m00 = m[0],
    m10 = m[1],
    m20 = m[2],
    m30 = m[3];
  const m02 = m[8],
    m12 = m[9],
    m22 = m[10],
    m32 = m[11];
  if (dst !== m) {
    dst[4] = m[4];
    dst[5] = m[5];
    dst[6] = m[6];
    dst[7] = m[7];
    dst[12] = m[12];
    dst[13] = m[13];
    dst[14] = m[14];
    dst[15] = m[15];
  }
  dst[0] = c * m00 - s * m02;
  dst[1] = c * m10 - s * m12;
  dst[2] = c * m20 - s * m22;
  dst[3] = c * m30 - s * m32;
  dst[8] = s * m00 + c * m02;
  dst[9] = s * m10 + c * m12;
  dst[10] = s * m20 + c * m22;
  dst[11] = s * m30 + c * m32;
};

// WebGPU-style perspective: right-handed, output z mapped to [0, 1]. fovy is
// the vertical field of view in radians.
const setPerspective = (
  out: Float32Array,
  fovy: number,
  aspect: number,
  near: number,
  far: number,
) => {
  "worklet";
  const f = 1 / Math.tan(fovy * 0.5);
  const rangeInv = 1 / (near - far);
  out[0] = f / aspect;
  out[1] = 0;
  out[2] = 0;
  out[3] = 0;
  out[4] = 0;
  out[5] = f;
  out[6] = 0;
  out[7] = 0;
  out[8] = 0;
  out[9] = 0;
  out[10] = far * rangeInv;
  out[11] = -1;
  out[12] = 0;
  out[13] = 0;
  out[14] = far * near * rangeInv;
  out[15] = 0;
};

// View matrix (RH). Camera looks down -z in its local frame. Same layout
// wgpu-matrix's lookAt produces, with the translation column carrying the
// negative basis-dot-eye terms.
const setLookAt = (
  out: Float32Array,
  ex: number,
  ey: number,
  ez: number,
  tx: number,
  ty: number,
  tz: number,
  ux: number,
  uy: number,
  uz: number,
) => {
  "worklet";
  let zx = ex - tx;
  let zy = ey - ty;
  let zz = ez - tz;
  const zLen = Math.hypot(zx, zy, zz);
  zx /= zLen;
  zy /= zLen;
  zz /= zLen;

  let xx = uy * zz - uz * zy;
  let xy = uz * zx - ux * zz;
  let xz = ux * zy - uy * zx;
  const xLen = Math.hypot(xx, xy, xz);
  xx /= xLen;
  xy /= xLen;
  xz /= xLen;

  const yx = zy * xz - zz * xy;
  const yy = zz * xx - zx * xz;
  const yz = zx * xy - zy * xx;

  out[0] = xx;
  out[1] = yx;
  out[2] = zx;
  out[3] = 0;
  out[4] = xy;
  out[5] = yy;
  out[6] = zy;
  out[7] = 0;
  out[8] = xz;
  out[9] = yz;
  out[10] = zz;
  out[11] = 0;
  out[12] = -(xx * ex + xy * ey + xz * ez);
  out[13] = -(yx * ex + yy * ey + yz * ez);
  out[14] = -(zx * ex + zy * ey + zz * ez);
  out[15] = 1;
};

// dst = a * b. Safe with dst === a or dst === b (snapshots both fully first).
const multiplyMat4 = (a: Float32Array, b: Float32Array, dst: Float32Array) => {
  "worklet";
  const a00 = a[0],
    a10 = a[1],
    a20 = a[2],
    a30 = a[3];
  const a01 = a[4],
    a11 = a[5],
    a21 = a[6],
    a31 = a[7];
  const a02 = a[8],
    a12 = a[9],
    a22 = a[10],
    a32 = a[11];
  const a03 = a[12],
    a13 = a[13],
    a23 = a[14],
    a33 = a[15];
  const b00 = b[0],
    b10 = b[1],
    b20 = b[2],
    b30 = b[3];
  const b01 = b[4],
    b11 = b[5],
    b21 = b[6],
    b31 = b[7];
  const b02 = b[8],
    b12 = b[9],
    b22 = b[10],
    b32 = b[11];
  const b03 = b[12],
    b13 = b[13],
    b23 = b[14],
    b33 = b[15];

  dst[0] = a00 * b00 + a01 * b10 + a02 * b20 + a03 * b30;
  dst[1] = a10 * b00 + a11 * b10 + a12 * b20 + a13 * b30;
  dst[2] = a20 * b00 + a21 * b10 + a22 * b20 + a23 * b30;
  dst[3] = a30 * b00 + a31 * b10 + a32 * b20 + a33 * b30;
  dst[4] = a00 * b01 + a01 * b11 + a02 * b21 + a03 * b31;
  dst[5] = a10 * b01 + a11 * b11 + a12 * b21 + a13 * b31;
  dst[6] = a20 * b01 + a21 * b11 + a22 * b21 + a23 * b31;
  dst[7] = a30 * b01 + a31 * b11 + a32 * b21 + a33 * b31;
  dst[8] = a00 * b02 + a01 * b12 + a02 * b22 + a03 * b32;
  dst[9] = a10 * b02 + a11 * b12 + a12 * b22 + a13 * b32;
  dst[10] = a20 * b02 + a21 * b12 + a22 * b22 + a23 * b32;
  dst[11] = a30 * b02 + a31 * b12 + a32 * b22 + a33 * b32;
  dst[12] = a00 * b03 + a01 * b13 + a02 * b23 + a03 * b33;
  dst[13] = a10 * b03 + a11 * b13 + a12 * b23 + a13 * b33;
  dst[14] = a20 * b03 + a21 * b13 + a22 * b23 + a23 * b33;
  dst[15] = a30 * b03 + a31 * b13 + a32 * b23 + a33 * b33;
};

// The 3D variant of the VisionCamera demo. Reuses the same shared-texture-memory
// pipeline to import camera frames as GPUExternalTextures, but instead of
// applying 2D effects, it samples the camera as a spherical environment map on
// a chrome sphere, an orbiting cube, and a torus.

const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

// Android-only feature; same probe as VisionCamera.tsx. Without it Dawn can't
// wrap a YCbCr AHB as a GPUExternalTexture.
const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

const DEPTH_FORMAT: GPUTextureFormat = "depth24plus";

// Backdrop blur tuning. Matches VisionCamera's "Strong" preset: prepass to a
// 1/4-res rgba8unorm, then 3 H-V iterations of the tile-based box blur. The
// final result is sampled by BACKDROP_SHADER as a fullscreen backdrop.
const BLUR_SCALE = 4;
const BLUR_FILTER_SIZE = 31;
const BLUR_TILE_DIM = 128;
const BLUR_BATCH = 4;
const BLUR_BLOCK_DIM = BLUR_TILE_DIM - BLUR_FILTER_SIZE;
const BLUR_ITERATIONS = 3;

// Scene UBO layout. mat4(64) + vec4(16) + vec4(16) = 96 bytes; pad to 16-byte
// alignment for safety.
const SCENE_UBO_SIZE = 96;
const SCENE_UBO_FLOATS = SCENE_UBO_SIZE / 4;
const OBJECT_UBO_SIZE = 64; // mat4

type Shape = {
  vertexBuffer: GPUBuffer;
  indexBuffer: GPUBuffer;
  indexCount: number;
  uniformBuffer: GPUBuffer;
  // Returns a model matrix for time t (seconds).
  modelAt: (t: number, out: Float32Array) => void;
};

export const ChromeSphere = () => {
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
  return <SceneView />;
};

const SceneView = () => {
  const ref = useCanvasRef();
  const [gpu, setGpu] = React.useState<{
    adapter: GPUAdapter;
    device: GPUDevice;
  } | null>(null);
  const [deviceError, setDeviceError] = React.useState<string | null>(null);
  React.useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
          throw new Error("requestAdapter returned null");
        }
        const hasOpaqueYCbCrExt =
          Platform.OS !== "android" || adapter.features.has(OPAQUE_YCBCR_EXT);
        if (Platform.OS === "android" && !hasOpaqueYCbCrExt) {
          throw new Error(
            "This Android device's Vulkan driver doesn't advertise " +
              "opaque-ycbcr-android-for-external-texture. Camera-frame import " +
              "as a GPUExternalTexture isn't supported here.",
          );
        }
        const featuresToRequest: GPUFeatureName[] = [
          ...REQUIRED_FEATURES,
          ...(Platform.OS === "android" ? [OPAQUE_YCBCR_EXT] : []),
        ];
        const device = await adapter.requestDevice({
          requiredFeatures: featuresToRequest,
        });
        if (cancelled) {
          return;
        }
        setGpu({ adapter, device });
      } catch (e) {
        if (cancelled) {
          return;
        }
        setDeviceError(String(e));
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);
  const device = gpu?.device ?? null;
  const adapter = gpu?.adapter ?? null;
  const devices = useCameraDevices();
  const cameraDevice = React.useMemo(
    () =>
      devices.find((d) => d.position === "back") ??
      devices.find((d) => d.position === "front") ??
      devices[0],
    [devices],
  );

  const [pipelineState, setPipelineState] = React.useState<{
    pipeline: GPURenderPipeline;
    sampler: GPUSampler;
    sceneUniformBuffer: GPUBuffer;
    depthView: GPUTextureView;
    context: RNCanvasContext;
    canvasWidth: number;
    canvasHeight: number;
    shapes: Shape[];
    // Pre-allocated scratch so the worklet doesn't allocate per frame. These
    // get mutated each tick, which is safe because the worklet sees its own
    // copy after closure serialization.
    view: Float32Array;
    proj: Float32Array;
    viewProj: Float32Array;
    sceneData: Float32Array;
    modelScratch: Float32Array;
    // Blurred-camera backdrop infrastructure.
    backdropPipeline: GPURenderPipeline;
    backdropBindGroup: GPUBindGroup;
    prepassPipeline: GPURenderPipeline;
    prepassUniformBuffer: GPUBuffer;
    blurPipeline: GPUComputePipeline;
    blurConstants: GPUBindGroup;
    blurBindGroup0: GPUBindGroup;
    blurBindGroup1: GPUBindGroup;
    blurBindGroup2: GPUBindGroup;
    blurSrcTexture: GPUTexture;
    blurWidth: number;
    blurHeight: number;
  } | null>(null);
  const [error, setError] = React.useState<string | null>(null);

  useEffect(() => {
    if (!device || pipelineState) {
      return;
    }
    const missing = REQUIRED_FEATURES.filter((f) => !device.features.has(f));
    if (missing.length > 0) {
      setError(
        `Device missing features [${missing.join(", ")}]. Adapter: ${
          adapter
            ? [...adapter.features]
                .filter((f) => f.toString().startsWith("shared-"))
                .join(", ") || "none"
            : "n/a"
        }`,
      );
      return;
    }
    const context = ref.current?.getContext("webgpu");
    if (!context) {
      return;
    }
    const canvas = context.canvas as unknown as NativeCanvas;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    context.configure({
      device,
      format: presentationFormat,
      alphaMode: "premultiplied",
    });

    const module = device.createShaderModule({ code: SHADER });
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: {
        module,
        entryPoint: "vs_main",
        buffers: [
          {
            arrayStride: VERTEX_STRIDE_BYTES,
            attributes: [
              {
                shaderLocation: 0,
                offset: POSITION_OFFSET,
                format: "float32x3",
              },
              {
                shaderLocation: 1,
                offset: NORMAL_OFFSET,
                format: "float32x3",
              },
            ],
          },
        ],
      },
      fragment: {
        module,
        entryPoint: "fs_main",
        targets: [{ format: presentationFormat }],
      },
      primitive: { topology: "triangle-list", cullMode: "back" },
      depthStencil: {
        depthCompare: "less",
        depthWriteEnabled: true,
        format: DEPTH_FORMAT,
      },
    });
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
    });
    const sceneUniformBuffer = device.createBuffer({
      size: SCENE_UBO_SIZE,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const depthTexture = device.createTexture({
      size: [canvas.width, canvas.height],
      format: DEPTH_FORMAT,
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const sphereMesh = generateSphere(1.0, 48, 64);

    const buildShape = (
      mesh: { vertices: Float32Array; indices: Uint16Array },
      modelAt: (t: number, out: Float32Array) => void,
    ): Shape => {
      const vertexBuffer = device.createBuffer({
        size: mesh.vertices.byteLength,
        usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(vertexBuffer, 0, mesh.vertices);
      // Index buffers must be sized to a multiple of 4 bytes; pad Uint16Array
      // by one extra index if needed.
      const indexByteLength = (mesh.indices.byteLength + 3) & ~3;
      const indexBuffer = device.createBuffer({
        size: indexByteLength,
        usage: GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(indexBuffer, 0, mesh.indices);
      const uniformBuffer = device.createBuffer({
        size: OBJECT_UBO_SIZE,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });
      // The full bind group is built per-frame in the worklet because
      // GPUExternalTexture is single-use; scene + object buffers above stay
      // alive across frames and feed back into that per-frame build.
      return {
        vertexBuffer,
        indexBuffer,
        indexCount: mesh.indices.length,
        uniformBuffer,
        modelAt,
      };
    };

    // Single chrome sphere center stage with a slow Y-rotation so the
    // reflection drifts even when the orbit camera is between key positions.
    // Runs inside the frame worklet.
    const shapes: Shape[] = [
      buildShape(sphereMesh, (t, out) => {
        "worklet";
        setIdentity(out);
        applyRotateY(out, t * 0.25, out);
      }),
    ];

    // ----- Backdrop blur infrastructure (same as VisionCamera "Strong") ---
    const blurWidth = Math.max(
      BLUR_TILE_DIM,
      Math.ceil(canvas.width / BLUR_SCALE),
    );
    const blurHeight = Math.max(
      BLUR_TILE_DIM,
      Math.ceil(canvas.height / BLUR_SCALE),
    );

    const prepassModule = device.createShaderModule({ code: PREPASS_SHADER });
    const prepassPipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: prepassModule, entryPoint: "vs_main" },
      fragment: {
        module: prepassModule,
        entryPoint: "fs_main",
        targets: [{ format: "rgba8unorm" }],
      },
      primitive: { topology: "triangle-list" },
    });
    const prepassUniformBuffer = device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const blurSrcTexture = device.createTexture({
      size: [blurWidth, blurHeight],
      format: "rgba8unorm",
      usage:
        GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
    });
    const blurPing = [0, 1].map(() =>
      device.createTexture({
        size: [blurWidth, blurHeight],
        format: "rgba8unorm",
        usage:
          GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.TEXTURE_BINDING,
      }),
    );

    const blurPipeline = device.createComputePipeline({
      layout: "auto",
      compute: {
        module: device.createShaderModule({ code: BLUR_SHADER }),
        entryPoint: "main",
      },
    });

    const flip0Buffer = device.createBuffer({
      size: 4,
      mappedAtCreation: true,
      usage: GPUBufferUsage.UNIFORM,
    });
    new Uint32Array(flip0Buffer.getMappedRange())[0] = 0;
    flip0Buffer.unmap();
    const flip1Buffer = device.createBuffer({
      size: 4,
      mappedAtCreation: true,
      usage: GPUBufferUsage.UNIFORM,
    });
    new Uint32Array(flip1Buffer.getMappedRange())[0] = 1;
    flip1Buffer.unmap();

    const blurParamsBuffer = device.createBuffer({
      size: 8,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    device.queue.writeBuffer(
      blurParamsBuffer,
      0,
      new Uint32Array([BLUR_FILTER_SIZE + 1, BLUR_BLOCK_DIM]),
    );

    const blurConstants = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: sampler },
        { binding: 1, resource: { buffer: blurParamsBuffer } },
      ],
    });
    // H: blurSrcTexture -> blurPing[0]
    const blurBindGroup0 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: blurSrcTexture.createView() },
        { binding: 2, resource: blurPing[0].createView() },
        { binding: 3, resource: { buffer: flip0Buffer } },
      ],
    });
    // V: blurPing[0] -> blurPing[1]
    const blurBindGroup1 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: blurPing[0].createView() },
        { binding: 2, resource: blurPing[1].createView() },
        { binding: 3, resource: { buffer: flip1Buffer } },
      ],
    });
    // H (iteration N>=2): blurPing[1] -> blurPing[0]
    const blurBindGroup2 = device.createBindGroup({
      layout: blurPipeline.getBindGroupLayout(1),
      entries: [
        { binding: 1, resource: blurPing[1].createView() },
        { binding: 2, resource: blurPing[0].createView() },
        { binding: 3, resource: { buffer: flip0Buffer } },
      ],
    });
    // Final iteration's V pass always lands in blurPing[1].
    const blurredView = blurPing[1].createView();

    // Backdrop pipeline: shares the render pass with the chrome sphere, so
    // it must declare a matching depth-stencil layout. depthCompare always /
    // depthWriteEnabled false means it draws unconditionally and never
    // disturbs depth for the subsequent sphere draw.
    const backdropModule = device.createShaderModule({ code: BACKDROP_SHADER });
    const backdropPipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: backdropModule, entryPoint: "vs_main" },
      fragment: {
        module: backdropModule,
        entryPoint: "fs_main",
        targets: [{ format: presentationFormat }],
      },
      primitive: { topology: "triangle-list" },
      depthStencil: {
        depthCompare: "always",
        depthWriteEnabled: false,
        format: DEPTH_FORMAT,
      },
    });
    const backdropBindGroup = device.createBindGroup({
      layout: backdropPipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: blurredView },
        { binding: 1, resource: sampler },
      ],
    });

    setPipelineState({
      pipeline,
      sampler,
      sceneUniformBuffer,
      depthView: depthTexture.createView(),
      context,
      canvasWidth: canvas.width,
      canvasHeight: canvas.height,
      shapes,
      view: new Float32Array(16),
      proj: new Float32Array(16),
      viewProj: new Float32Array(16),
      sceneData: new Float32Array(SCENE_UBO_FLOATS),
      modelScratch: new Float32Array(16),
      backdropPipeline,
      backdropBindGroup,
      prepassPipeline,
      prepassUniformBuffer,
      blurPipeline,
      blurConstants,
      blurBindGroup0,
      blurBindGroup1,
      blurBindGroup2,
      blurSrcTexture,
      blurWidth,
      blurHeight,
    });
  }, [device, adapter, ref, pipelineState]);

  const startTimeRef = React.useRef<number>(performance.now());
  const logBox = React.useMemo(() => ({ seen: false }), []);

  const frameOutput = useFrameOutput({
    pixelFormat: "native",
    onFrame: (frame) => {
      "worklet";
      if (!logBox.seen) {
        logBox.seen = true;
        console.log(
          "[ChromeSphere] worklet first frame, hasPipeline=" +
            String(pipelineState != null) +
            " frame=" +
            String(frame.width) +
            "x" +
            String(frame.height),
        );
      }
      if (!pipelineState || !device) {
        frame.dispose();
        return;
      }
      const {
        pipeline,
        sampler,
        sceneUniformBuffer,
        depthView,
        context,
        canvasWidth,
        canvasHeight,
        shapes,
        view,
        proj,
        viewProj,
        sceneData,
        modelScratch,
        backdropPipeline,
        backdropBindGroup,
        prepassPipeline,
        prepassUniformBuffer,
        blurPipeline,
        blurConstants,
        blurBindGroup0,
        blurBindGroup1,
        blurBindGroup2,
        blurSrcTexture,
        blurWidth,
        blurHeight,
      } = pipelineState;
      const nativeBuffer = frame.getNativeBuffer();
      try {
        const videoFrame = device.createVideoFrameFromNativeBuffer(
          nativeBuffer.pointer,
        );
        try {
          const t = (performance.now() - startTimeRef.current) / 1000;

          // Orbit the eye around the origin, looking at it. Small bob in y
          // so reflections shift along the polar axis too.
          const orbitR = 5.2;
          const ex = Math.cos(t * 0.15) * orbitR;
          const ey = 1.2 + Math.sin(t * 0.2) * 0.4;
          const ez = Math.sin(t * 0.15) * orbitR;
          setLookAt(view, ex, ey, ez, 0.0, 0.0, 0.0, 0, 1, 0);
          setPerspective(
            proj,
            Math.PI / 4,
            canvasWidth / canvasHeight,
            0.1,
            100,
          );
          multiplyMat4(proj, view, viewProj);

          sceneData.set(viewProj, 0);
          sceneData[16] = ex;
          sceneData[17] = ey;
          sceneData[18] = ez;
          sceneData[19] = 0;
          // Key light rises and slowly drifts so the chrome specular sweeps
          // across the silhouettes.
          const ldRawX = Math.cos(t * 0.3) * 0.6;
          const ldRawY = 0.8;
          const ldRawZ = Math.sin(t * 0.3) * 0.6;
          const ldLen = Math.hypot(ldRawX, ldRawY, ldRawZ);
          sceneData[20] = ldRawX / ldLen;
          sceneData[21] = ldRawY / ldLen;
          sceneData[22] = ldRawZ / ldLen;
          sceneData[23] = 0;
          device.queue.writeBuffer(sceneUniformBuffer, 0, sceneData);

          for (const shape of shapes) {
            shape.modelAt(t, modelScratch);
            device.queue.writeBuffer(shape.uniformBuffer, 0, modelScratch);
          }

          let externalTex;
          try {
            externalTex = device.importExternalTexture({
              source: videoFrame,
              label: "chrome-env",
            });
          } catch (e) {
            console.warn(
              "[ChromeSphere] importExternalTexture threw: " + String(e),
            );
            throw e;
          }

          const encoder = device.createCommandEncoder();

          // ---- Backdrop blur (prepass + 3 H-V iterations at 1/4 res) ----
          device.queue.writeBuffer(
            prepassUniformBuffer,
            0,
            new Float32Array([
              videoFrame.width,
              videoFrame.height,
              canvasWidth,
              canvasHeight,
            ]),
          );
          const prepassBindGroup = device.createBindGroup({
            layout: prepassPipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: externalTex },
              { binding: 1, resource: sampler },
              { binding: 2, resource: { buffer: prepassUniformBuffer } },
            ],
          });
          const prepass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: blurSrcTexture.createView(),
                clearValue: { r: 0, g: 0, b: 0, a: 1 },
                loadOp: "clear",
                storeOp: "store",
              },
            ],
          });
          prepass.setPipeline(prepassPipeline);
          prepass.setBindGroup(0, prepassBindGroup);
          prepass.draw(3);
          prepass.end();

          const compute = encoder.beginComputePass();
          compute.setPipeline(blurPipeline);
          compute.setBindGroup(0, blurConstants);
          compute.setBindGroup(1, blurBindGroup0);
          compute.dispatchWorkgroups(
            Math.ceil(blurWidth / BLUR_BLOCK_DIM),
            Math.ceil(blurHeight / BLUR_BATCH),
          );
          compute.setBindGroup(1, blurBindGroup1);
          compute.dispatchWorkgroups(
            Math.ceil(blurHeight / BLUR_BLOCK_DIM),
            Math.ceil(blurWidth / BLUR_BATCH),
          );
          for (let i = 0; i < BLUR_ITERATIONS - 1; i++) {
            compute.setBindGroup(1, blurBindGroup2);
            compute.dispatchWorkgroups(
              Math.ceil(blurWidth / BLUR_BLOCK_DIM),
              Math.ceil(blurHeight / BLUR_BATCH),
            );
            compute.setBindGroup(1, blurBindGroup1);
            compute.dispatchWorkgroups(
              Math.ceil(blurHeight / BLUR_BLOCK_DIM),
              Math.ceil(blurWidth / BLUR_BATCH),
            );
          }
          compute.end();

          // ---- Main scene pass: backdrop first (no depth write), then sphere
          const pass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: context.getCurrentTexture().createView(),
                clearValue: { r: 0, g: 0, b: 0, a: 1 },
                loadOp: "clear",
                storeOp: "store",
              },
            ],
            depthStencilAttachment: {
              view: depthView,
              depthClearValue: 1.0,
              depthLoadOp: "clear",
              depthStoreOp: "store",
            },
          });

          // Backdrop pipeline has depthCompare: "always" and writes nothing
          // to depth, so the subsequent sphere draw still sees a clean depth
          // buffer with the canvas-clear far value.
          pass.setPipeline(backdropPipeline);
          pass.setBindGroup(0, backdropBindGroup);
          pass.draw(3);

          pass.setPipeline(pipeline);
          for (const shape of shapes) {
            // The external texture is bound per-shape (one bind group per
            // shape, the only difference being the per-object uniform), so
            // we rebuild the bind group each frame to splice in this frame's
            // externalTex alongside the cached scene + object buffers.
            const bindGroup = device.createBindGroup({
              layout: pipeline.getBindGroupLayout(0),
              entries: [
                { binding: 0, resource: { buffer: sceneUniformBuffer } },
                { binding: 1, resource: { buffer: shape.uniformBuffer } },
                { binding: 2, resource: externalTex },
                { binding: 3, resource: sampler },
              ],
            });
            pass.setBindGroup(0, bindGroup);
            pass.setVertexBuffer(0, shape.vertexBuffer);
            pass.setIndexBuffer(shape.indexBuffer, "uint16");
            pass.drawIndexed(shape.indexCount);
          }
          pass.end();
          device.queue.submit([encoder.finish()]);
          context.present();
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

  if (deviceError) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>
          Device creation failed: {deviceError}
        </Text>
      </View>
    );
  }
  if (error) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>{error}</Text>
      </View>
    );
  }
  if (!device) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>Waiting for GPU device...</Text>
      </View>
    );
  }
  if (cameraDevice == null) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>
          No camera available. This screen needs a physical device with a camera
          (the iOS Simulator does not have one).
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
