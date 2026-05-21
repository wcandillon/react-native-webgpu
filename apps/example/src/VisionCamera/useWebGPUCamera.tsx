import React, { useEffect, useState } from "react";
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

// Camera frame -> SharedTextureMemory (NV12 biplanar) -> GPUExternalTexture
// with hardware YUV/sRGB conversion. The worklet path relies on
// react-native-wgpu's registerWebGPUForReanimated (auto-invoked on import in
// the main runtime) to make GPUDevice / canvas / pipeline / sampler / buffer
// auto-(un)box across the worklet boundary.

const BASE_REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

// Android-only feature, gates Dawn's "wrap a YCbCr AHB as a GPUExternalTexture
// with implicit SamplerYcbcrConversion" path. Without it our native
// importExternalTexture flow on Android can't produce a usable external
// texture from a camera frame.
const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

export interface WebGPUCameraSetupInfo {
  device: GPUDevice;
  context: RNCanvasContext;
  presentationFormat: GPUTextureFormat;
  canvasWidth: number;
  canvasHeight: number;
}

export interface WebGPUCameraFrameInfo<TPipelineState> {
  device: GPUDevice;
  context: RNCanvasContext;
  externalTexture: GPUExternalTexture;
  canvasWidth: number;
  canvasHeight: number;
  frameWidth: number;
  frameHeight: number;
  pipelineState: TPipelineState;
}

export interface UseWebGPUCameraOptions<TPipelineState> {
  requiredFeatures?: GPUFeatureName[];
  // Build per-demo pipeline state on the main thread once device + canvas are
  // ready. The returned value is captured into the frame processor worklet
  // closure via the WebGPU custom serializer, so it should be composed of
  // serializable GPU objects (pipeline, sampler, buffer, texture view, etc.)
  // plus plain JSON-friendly values.
  setup: (
    info: WebGPUCameraSetupInfo,
  ) => TPipelineState | Promise<TPipelineState>;
  // Called per frame inside the Vision Camera worklet runtime, after the
  // camera frame has been imported as a GPUExternalTexture. The hook handles
  // release/dispose of the underlying native buffer and video frame.
  render: (frame: WebGPUCameraFrameInfo<TPipelineState>) => void;
}

export interface UseWebGPUCameraResult {
  element: React.ReactElement;
  device: GPUDevice | null;
}

export const useWebGPUCamera = <TPipelineState,>(
  options: UseWebGPUCameraOptions<TPipelineState>,
): UseWebGPUCameraResult => {
  const { requiredFeatures, setup, render } = options;

  const { hasPermission, requestPermission } = useCameraPermission();
  useEffect(() => {
    if (!hasPermission) {
      requestPermission();
    }
  }, [hasPermission, requestPermission]);

  const canvasRef = useCanvasRef();

  const [gpu, setGpu] = useState<{
    adapter: GPUAdapter;
    device: GPUDevice;
  } | null>(null);
  const [deviceError, setDeviceError] = useState<string | null>(null);

  useEffect(() => {
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
              "as a GPUExternalTexture isn't supported here. (This is a " +
              "device/driver limitation, not a code issue.)",
          );
        }
        const featuresToRequest: GPUFeatureName[] = [
          ...BASE_REQUIRED_FEATURES,
          ...(requiredFeatures ?? []),
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
        console.warn("[useWebGPUCamera] device creation failed: " + String(e));
        setDeviceError(String(e));
      }
    })();
    return () => {
      cancelled = true;
    };
    // requiredFeatures is intentionally read once on mount; changing it would
    // require a full device teardown which isn't a use case we care about.
    // eslint-disable-next-line react-hooks/exhaustive-deps
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

  const [pipelineState, setPipelineState] = useState<{
    state: TPipelineState;
    context: RNCanvasContext;
    canvasWidth: number;
    canvasHeight: number;
  } | null>(null);
  const [setupError, setSetupError] = useState<string | null>(null);

  // We only want to run setup once per device. The user's setup callback is
  // captured in a ref so re-renders that change the callback identity don't
  // re-trigger setup.
  const setupRef = React.useRef(setup);
  React.useEffect(() => {
    setupRef.current = setup;
  }, [setup]);

  useEffect(() => {
    if (!device || pipelineState || !hasPermission) {
      return;
    }
    const allRequired = [
      ...BASE_REQUIRED_FEATURES,
      ...(requiredFeatures ?? []),
    ];
    const missing = allRequired.filter((f) => !device.features.has(f));
    if (missing.length > 0) {
      setSetupError(
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
    const context = canvasRef.current?.getContext("webgpu");
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
    let cancelled = false;
    (async () => {
      try {
        const state = await setupRef.current({
          device,
          context,
          presentationFormat,
          canvasWidth: canvas.width,
          canvasHeight: canvas.height,
        });
        if (cancelled) {
          return;
        }
        setPipelineState({
          state,
          context,
          canvasWidth: canvas.width,
          canvasHeight: canvas.height,
        });
      } catch (e) {
        if (cancelled) {
          return;
        }
        console.warn("[useWebGPUCamera] setup threw: " + String(e));
        setSetupError(String(e));
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [
    device,
    adapter,
    canvasRef,
    pipelineState,
    hasPermission,
    requiredFeatures,
  ]);

  const frameOutput = useFrameOutput({
    pixelFormat: "native",
    onFrame: (frame) => {
      "worklet";
      if (!pipelineState || !device) {
        frame.dispose();
        return;
      }
      const { state, context, canvasWidth, canvasHeight } = pipelineState;
      const nativeBuffer = frame.getNativeBuffer();
      try {
        const videoFrame = device.createVideoFrameFromNativeBuffer(
          nativeBuffer.pointer,
        );
        try {
          const externalTexture = device.importExternalTexture({
            source: videoFrame,
            label: "camera-frame",
          });
          render({
            device,
            context,
            externalTexture,
            canvasWidth,
            canvasHeight,
            frameWidth: videoFrame.width,
            frameHeight: videoFrame.height,
            pipelineState: state,
          });
        } finally {
          videoFrame.release();
        }
      } finally {
        nativeBuffer.release();
        frame.dispose();
      }
    },
  });

  // useCamera must be called unconditionally for stable hook order. Pass a
  // stub device when none exists; isActive stays false so the session never
  // tries to start.
  useCamera({
    isActive: hasPermission && pipelineState != null && cameraDevice != null,
    device: cameraDevice as NonNullable<typeof cameraDevice>,
    outputs: [frameOutput],
  });

  let element: React.ReactElement;
  if (!hasPermission) {
    element = (
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
  } else if (deviceError) {
    element = (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>
          Device creation failed: {deviceError}
        </Text>
      </View>
    );
  } else if (setupError) {
    element = (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>{setupError}</Text>
      </View>
    );
  } else if (!device) {
    element = (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>Waiting for GPU device...</Text>
      </View>
    );
  } else if (cameraDevice == null) {
    element = (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>
          No camera available. This screen needs a physical device with a camera
          (the iOS Simulator does not have one).
        </Text>
      </View>
    );
  } else {
    element = (
      <View style={styles.canvasContainer}>
        <Canvas ref={canvasRef} style={styles.canvas} />
      </View>
    );
  }

  return { element, device };
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
  canvasContainer: { flex: 1, backgroundColor: "black" },
  canvas: { flex: 1 },
});
