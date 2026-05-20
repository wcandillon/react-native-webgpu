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
  // z:    chromatic aberration offset in UV units.
  // w:    unused, padding.
  scaleAndAberration: vec4f,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

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

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let uvScale = u.scaleAndAberration.xy;
  let aberration = u.scaleAndAberration.z;
  let uv = vec2f(0.5) + (in.uv - vec2f(0.5)) * uvScale;
  // RGB split: sample red shifted right, blue shifted left, green centered.
  let r = textureSampleBaseClampToEdge(srcTex, srcSampler, uv + vec2f( aberration, 0.0)).r;
  let g = textureSampleBaseClampToEdge(srcTex, srcSampler, uv).g;
  let b = textureSampleBaseClampToEdge(srcTex, srcSampler, uv + vec2f(-aberration, 0.0)).b;
  return vec4f(r, g, b, 1.0);
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

const ABERRATION_STRENGTH = 0.006;

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
  } | null>(null);
  const [error, setError] = React.useState<string | null>(null);

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
      size: 16,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    setPipelineState({
      pipeline,
      sampler,
      uniformBuffer,
      context,
      canvasWidth: canvas.width,
      canvasHeight: canvas.height,
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
      } = pipelineState;
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
          device.queue.writeBuffer(
            uniformBuffer,
            0,
            new Float32Array([sx, sy, ABERRATION_STRENGTH, 0]),
          );

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
            ],
          });

          const encoder = device.createCommandEncoder();
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
    <View style={{ flex: 1, backgroundColor: "black" }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};

const styles = StyleSheet.create({
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
