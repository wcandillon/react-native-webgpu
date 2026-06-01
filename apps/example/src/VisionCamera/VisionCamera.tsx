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

import { BLUR_SHADER, PREPASS_SHADER } from "./blurShaders";
import { EffectToolbar } from "./EffectToolbar";
import {
  ABERRATION_STRENGTHS,
  BLUR_ITERATIONS,
  INITIAL_MODES,
  PIXELATE_BLOCKS,
  type Modes,
} from "./features";

// Camera frame → SharedTextureMemory (NV12 biplanar) → GPUExternalTexture →
// textureSampleBaseClampToEdge with hardware YUV/sRGB conversion → chromatic
// aberration in WGSL.
//
// Everything past frame arrival runs on Vision Camera's worklet runtime.
// react-native-wgpu's `registerWebGPUForReanimated` (loaded from main on
// startup) registers a Worklets custom serializer for GPUDevice / canvas /
// pipeline / sampler / buffer, so the closure references below auto-box on
// the way into the worklet and auto-unbox on the way out.

const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  // x, y: 'cover'-fit UV scale around (0.5, 0.5).
  // z:    chromatic aberration offset in UV units (0 disables).
  // w:    pixelate block size in UV units (0 disables).
  params: vec4f,
  // x: effect   (0 off, 1 gray, 2 sepia, 3 invert, 4 vibrant)
  // y: tint     (0 off, 1 warm, 2 cool)
  // z: vignette (0 off, 1 on)
  // w: blurMode (0 off, 1 strong - blurred everywhere (prepass bakes
  //              cover-fit), 2 overlay - blurred backdrop + sharp card)
  modes: vec4u,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;
@group(0) @binding(3) var blurredTex: texture_2d<f32>;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
  var positions = array<vec2f, 3>(
    vec2f(-1.0, -3.0),
    vec2f(-1.0,  1.0),
    vec2f( 3.0,  1.0),
  );
  var uvs = array<vec2f, 3>(
    vec2f(0.0, 2.0),
    vec2f(0.0, 0.0),
    vec2f(2.0, 0.0),
  );
  var out: VsOut;
  out.position = vec4f(positions[vid], 0.0, 1.0);
  out.uv = uvs[vid];
  return out;
}

fn snap(uv: vec2f, block: f32) -> vec2f {
  if (block <= 0.0) {
    return uv;
  }
  return (floor(uv / block) + vec2f(0.5)) * block;
}

fn sampleExternal(uv: vec2f, block: f32) -> vec4f {
  return textureSampleBaseClampToEdge(srcTex, srcSampler, snap(uv, block));
}

fn sampleBlurred(uv: vec2f, block: f32) -> vec4f {
  return textureSampleLevel(
    blurredTex,
    srcSampler,
    clamp(snap(uv, block), vec2f(0.0), vec2f(1.0)),
    0.0,
  );
}

// RGB-split sample of the external camera with cover-fit + optional pixelate.
fn rgbSplitExternal(uv: vec2f, aberration: f32, block: f32) -> vec3f {
  let r = sampleExternal(uv + vec2f( aberration, 0.0), block).r;
  let g = sampleExternal(uv, block).g;
  let b = sampleExternal(uv + vec2f(-aberration, 0.0), block).b;
  return vec3f(r, g, b);
}

// RGB-split sample of the pre-blurred 2D texture (cover-fit baked in).
fn rgbSplitBlurred(uv: vec2f, aberration: f32, block: f32) -> vec3f {
  let r = sampleBlurred(uv + vec2f( aberration, 0.0), block).r;
  let g = sampleBlurred(uv, block).g;
  let b = sampleBlurred(uv + vec2f(-aberration, 0.0), block).b;
  return vec3f(r, g, b);
}

fn applyEffect(rgb: vec3f, mode: u32) -> vec3f {
  if (mode == 1u) {
    let l = dot(rgb, vec3f(0.2126, 0.7152, 0.0722));
    return vec3f(l);
  }
  if (mode == 2u) {
    return vec3f(
      dot(rgb, vec3f(0.393, 0.769, 0.189)),
      dot(rgb, vec3f(0.349, 0.686, 0.168)),
      dot(rgb, vec3f(0.272, 0.534, 0.131))
    );
  }
  if (mode == 3u) {
    return vec3f(1.0) - rgb;
  }
  if (mode == 4u) {
    let l = dot(rgb, vec3f(0.2126, 0.7152, 0.0722));
    let sat = mix(vec3f(l), rgb, 1.55);
    return clamp((sat - 0.5) * 1.18 + 0.5, vec3f(0.0), vec3f(1.0));
  }
  return rgb;
}

fn applyTint(rgb: vec3f, mode: u32) -> vec3f {
  if (mode == 1u) {
    return clamp(rgb * vec3f(1.10, 1.02, 0.86), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 2u) {
    return clamp(rgb * vec3f(0.86, 0.98, 1.16), vec3f(0.0), vec3f(1.0));
  }
  return rgb;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let uvScale = u.params.xy;
  let aberration = u.params.z;
  let pixelate = u.params.w;
  let effect = u.modes.x;
  let tint = u.modes.y;
  let vignette = u.modes.z;
  let blurMode = u.modes.w;

  // Overlay card geometry. NDC-space rect, the camera image is cover-fit
  // inside the card (same uvScale as the off path), the blurred backdrop
  // fills outside.
  let overlayPadding = 0.08;
  let edgeAA = 0.004;

  var color: vec3f;
  if (blurMode == 1u) {
    // Strong: prepass already baked cover-fit, so feed in.uv straight in.
    color = rgbSplitBlurred(in.uv, aberration, pixelate);
  } else if (blurMode == 2u) {
    // Overlay: per-fragment edge factor 0 strictly inside the card,
    // 1 strictly outside, smoothstep band for AA. Uniform within the branch
    // (blurMode came from the uniform buffer), so calling
    // textureSampleBaseClampToEdge on the external texture is allowed.
    let cardHalf = vec2f(0.5 - overlayPadding);
    let p = abs(in.uv - vec2f(0.5));
    let edgeDist = max(p.x - cardHalf.x, p.y - cardHalf.y);
    let outside = smoothstep(-edgeAA, edgeAA, edgeDist);

    let cardUv = (in.uv - vec2f(overlayPadding)) /
                 (1.0 - 2.0 * overlayPadding);
    let sharpUv = vec2f(0.5) + (cardUv - vec2f(0.5)) * uvScale;
    let sharp = rgbSplitExternal(sharpUv, aberration, pixelate);
    let backdrop = rgbSplitBlurred(in.uv, aberration, pixelate);
    color = mix(sharp, backdrop, outside);
  } else {
    let uv = vec2f(0.5) + (in.uv - vec2f(0.5)) * uvScale;
    color = rgbSplitExternal(uv, aberration, pixelate);
  }

  color = applyEffect(color, effect);
  color = applyTint(color, tint);

  if (vignette == 1u) {
    let d = distance(in.uv, vec2f(0.5));
    let v = 1.0 - smoothstep(0.35, 0.85, d);
    color = color * v;
  }

  return vec4f(color, 1.0);
}
`;

const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

// Android-only feature, gates Dawn's "wrap a YCbCr AHB as a GPUExternalTexture
// with implicit SamplerYcbcrConversion" path. Without it our native
// `importExternalTexture` flow on Android can't produce a usable external
// texture from a camera frame. We probe the adapter for it and surface a
// clear error if the device's Vulkan driver doesn't advertise it (e.g. some
// Android-Desktop / Chromebook configurations).
const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

// Blur infrastructure: prepass writes the
// cover-fit camera image into a 1/4-res rgba8unorm, the separable box-blur
// compute pings between two storage textures, and the main pass linearly
// upsamples the final result so the effective sigma is ~4x the per-iteration
// kernel.
const BLUR_SCALE = 4;
const BLUR_FILTER_SIZE = 31;
const BLUR_TILE_DIM = 128;
const BLUR_BATCH = 4;
const BLUR_BLOCK_DIM = BLUR_TILE_DIM - BLUR_FILTER_SIZE;

export const VisionCamera = () => {
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
  return <CameraView />;
};

const CameraView = () => {
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
        const adapterFeatures = [...adapter.features].sort();
        console.log(
          "[VisionCamera] adapter features (" +
            adapterFeatures.length +
            "): " +
            adapterFeatures.join(", "),
        );
        const hasOpaqueYCbCrExt =
          Platform.OS !== "android" || adapter.features.has(OPAQUE_YCBCR_EXT);
        if (Platform.OS === "android" && !hasOpaqueYCbCrExt) {
          throw new Error(
            "This Android device's Vulkan driver doesn't advertise " +
              "opaque-ycbcr-android-for-external-texture. Camera-frame import " +
              "as a GPUExternalTexture isn't supported here. (This is a " +
              "device/driver limitation, not a code issue.)",
          );
        }
        const featuresToRequest: GPUFeatureName[] = [
          ...REQUIRED_FEATURES,
          ...(Platform.OS === "android" ? [OPAQUE_YCBCR_EXT] : []),
        ];
        console.log(
          "[VisionCamera] requesting device with features: " +
            featuresToRequest.join(", "),
        );
        const device = await adapter.requestDevice({
          requiredFeatures: featuresToRequest,
        });
        if (cancelled) {
          return;
        }
        console.log(
          "[VisionCamera] device created, features: " +
            [...device.features].sort().join(", "),
        );
        setGpu({ adapter, device });
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[VisionCamera] device creation failed: " + String(e));
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
  // Pick back camera if available, otherwise front, otherwise anything. The
  // iOS simulator returns an empty list since there are no cameras, in which
  // case we surface a clear error rather than letting useCamera throw.
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
    uniformBuffer: GPUBuffer;
    context: RNCanvasContext;
    canvasWidth: number;
    canvasHeight: number;
    prepassPipeline: GPURenderPipeline;
    prepassUniformBuffer: GPUBuffer;
    blurPipeline: GPUComputePipeline;
    blurConstants: GPUBindGroup;
    blurBindGroup0: GPUBindGroup;
    blurBindGroup1: GPUBindGroup;
    blurBindGroup2: GPUBindGroup;
    blurSrcTexture: GPUTexture;
    blurredView: GPUTextureView;
    blurWidth: number;
    blurHeight: number;
  } | null>(null);
  const [error, setError] = React.useState<string | null>(null);
  const [modes, setModes] = React.useState<Modes>(INITIAL_MODES);
  const cycle = React.useCallback((key: keyof Modes, optionsCount: number) => {
    setModes((prev) => ({ ...prev, [key]: (prev[key] + 1) % optionsCount }));
  }, []);

  // Initialize pipeline once device + canvas are both ready.
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
      vertex: { module, entryPoint: "vs_main" },
      fragment: {
        module,
        entryPoint: "fs_main",
        targets: [{ format: presentationFormat }],
      },
      primitive: { topology: "triangle-list" },
    });
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
    });
    const uniformBuffer = device.createBuffer({
      size: 32,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    // ----- Blur infrastructure -----
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

    setPipelineState({
      pipeline,
      sampler,
      uniformBuffer,
      context,
      canvasWidth: canvas.width,
      canvasHeight: canvas.height,
      prepassPipeline,
      prepassUniformBuffer,
      blurPipeline,
      blurConstants,
      blurBindGroup0,
      blurBindGroup1,
      blurBindGroup2,
      blurSrcTexture,
      blurredView,
      blurWidth,
      blurHeight,
    });
  }, [device, adapter, ref, pipelineState]);

  // Build the frame processor worklet. Captured WebGPU objects flow into the
  // worklet runtime via the registerWebGPUForReanimated custom serializer.
  // Diagnostic: log once on the *first call only* by capturing a plain bool
  // box. Worklets serializes the box once on closure creation, so flipping
  // .seen mutates the worklet-side copy; we use it strictly to avoid spamming
  // metro logs and don't rely on main-thread visibility.
  const logBox = React.useMemo(() => ({ seen: false }), []);
  const frameOutput = useFrameOutput({
    pixelFormat: "native", // zero-copy, gives us NV12 IOSurfaces on iOS
    onFrame: (frame) => {
      "worklet";
      if (!logBox.seen) {
        logBox.seen = true;

        console.log(
          "[VisionCamera] worklet first frame, hasPipeline=" +
            String(pipelineState != null) +
            " hasDevice=" +
            String(device != null) +
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
        uniformBuffer,
        context,
        canvasWidth,
        canvasHeight,
        prepassPipeline,
        prepassUniformBuffer,
        blurPipeline,
        blurConstants,
        blurBindGroup0,
        blurBindGroup1,
        blurBindGroup2,
        blurSrcTexture,
        blurredView,
        blurWidth,
        blurHeight,
      } = pipelineState;
      const blurIterations = BLUR_ITERATIONS[modes.blur] ?? 0;
      const blurMode = blurIterations > 0 ? modes.blur : 0;
      const nativeBuffer = frame.getNativeBuffer();
      try {
        let videoFrame;
        try {
          // Call createVideoFrameFromNativeBuffer on the device, not on the
          // RNWebGPU global — `device` is already box-able across worklet
          // runtimes via the WebGPU custom serializer (proven by the
          // Reanimated demo); RNWebGPU is a main-runtime-only global.
          videoFrame = device.createVideoFrameFromNativeBuffer(
            nativeBuffer.pointer,
          );
        } catch (e) {
          console.warn(
            "[VisionCamera] createVideoFrameFromNativeBuffer threw: " +
              String(e),
          );
          throw e;
        }
        try {
          // Compute cover-fit uvScale based on frame & canvas aspect ratios.
          // On most phones the back camera is landscape (e.g. 1920x1080) and
          // the canvas is portrait, so the y-axis gets cropped.
          const canvasAR = canvasWidth / canvasHeight;
          const frameAR = videoFrame.width / videoFrame.height;
          let sx = 1;
          let sy = 1;
          if (frameAR > canvasAR) {
            sx = canvasAR / frameAR;
          } else {
            sy = frameAR / canvasAR;
          }
          // 32-byte uniform: vec4f params + vec4u modes. Built on a single
          // ArrayBuffer so the f32/u32 halves go up in one writeBuffer call.
          const uniformData = new ArrayBuffer(32);
          const uniformF32 = new Float32Array(uniformData);
          const uniformU32 = new Uint32Array(uniformData);
          uniformF32[0] = sx;
          uniformF32[1] = sy;
          uniformF32[2] = ABERRATION_STRENGTHS[modes.aberration] ?? 0;
          uniformF32[3] = PIXELATE_BLOCKS[modes.pixelate] ?? 0;
          uniformU32[4] = modes.effect;
          uniformU32[5] = modes.tint;
          uniformU32[6] = modes.vignette;
          uniformU32[7] = blurMode;
          device.queue.writeBuffer(uniformBuffer, 0, uniformData);

          let externalTex;
          try {
            externalTex = device.importExternalTexture({
              source: videoFrame,
              label: "camera-frame",
            });
          } catch (e) {
            console.warn(
              "[VisionCamera] importExternalTexture threw: " + String(e),
            );
            throw e;
          }
          const bindGroup = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: externalTex },
              { binding: 1, resource: sampler },
              { binding: 2, resource: { buffer: uniformBuffer } },
              { binding: 3, resource: blurredView },
            ],
          });

          const encoder = device.createCommandEncoder();

          if (blurIterations > 0) {
            // Prepass: cover-fit external (YUV) -> rgba8unorm at 1/4 res.
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

            // Separable box-blur compute. Iteration 1 reads from
            // blurSrcTexture; subsequent iterations ping-pong between
            // blurPing[1] -> blurPing[0] -> blurPing[1].
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
            for (let i = 0; i < blurIterations - 1; i++) {
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
          }

          const pass = encoder.beginRenderPass({
            colorAttachments: [
              {
                view: context.getCurrentTexture().createView(),
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

  // We have to call useCamera unconditionally (hook order). Pass a stub
  // device when none exists so the hook doesn't throw, but keep isActive
  // false so it never tries to start the session.
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
      <EffectToolbar modes={modes} onCycle={cycle} />
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
