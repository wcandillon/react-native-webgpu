import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  PixelRatio,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import {
  Canvas,
  useCanvasRef,
  useDevice,
  type NativeCanvas,
  type VideoFrame,
} from "react-native-wgpu";

// importExternalTexture is the spec-mandated path for "I have a YUV-encoded
// video/camera frame and I want to sample it in a shader without copying or
// hand-rolling YUV math". The WGSL side uses texture_external +
// textureSampleBaseClampToEdge; the driver does the planar fetch, YUV→RGB
// matrix multiply, sRGB transfer, and gamut conversion in the sampler.
//
// This example layers five toggleable effects on top of the basic
// external-texture sample to showcase what you can do once the YUV frame is
// in WebGPU's hands:
//   * Resize modes (cover / contain / stretch / center 1:1) computed in WGSL
//     from the texture vs canvas aspect ratios.
//   * Color filters (grayscale / sepia / invert / vibrant) as 3x3 matrices.
//   * Color "grade" (warm / cool / wide-gamut-ish boost) layered on top.
//   * Ambient mode: fills the contain-mode bars with a Poisson-disk blur of
//     the same frame — same external texture, sampled many times per pixel.
//   * Liquid-glass control buttons drawn inside WGSL: rounded SDF lenses that
//     refract the underlying video by sampling texture_external at offset uvs.
const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  texSize: vec2f,
  canvasSize: vec2f,
  resizeMode: u32,
  shaderEffect: u32,
  colorSpace: u32,
  ambient: u32,
  liquidGlass: u32,
  time: f32,
  _pad: vec2f,
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

fn computeUvScale(mode: u32) -> vec2f {
  let canvasAR = u.canvasSize.x / u.canvasSize.y;
  let texAR = u.texSize.x / u.texSize.y;
  // 0 = cover, 1 = contain, 2 = stretch, 3 = center (1:1 pixel)
  if (mode == 0u) {
    if (texAR > canvasAR) {
      return vec2f(canvasAR / texAR, 1.0);
    }
    return vec2f(1.0, texAR / canvasAR);
  }
  if (mode == 1u) {
    if (texAR > canvasAR) {
      return vec2f(1.0, texAR / canvasAR);
    }
    return vec2f(canvasAR / texAR, 1.0);
  }
  if (mode == 3u) {
    return vec2f(u.canvasSize.x / u.texSize.x, u.canvasSize.y / u.texSize.y);
  }
  return vec2f(1.0, 1.0);
}

fn sampleTex(uv: vec2f) -> vec3f {
  return textureSampleBaseClampToEdge(srcTex, srcSampler, uv).rgb;
}

fn applyShaderEffect(rgb: vec3f, mode: u32) -> vec3f {
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

fn applyColorSpace(rgb: vec3f, mode: u32) -> vec3f {
  if (mode == 1u) {
    return clamp(rgb * vec3f(1.10, 1.02, 0.86), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 2u) {
    return clamp(rgb * vec3f(0.86, 0.98, 1.16), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 3u) {
    // Rec.709 -> Display P3 (approximate, clamped). Demonstrates a
    // primaries-conversion matrix on the GPU side after the YUV pipeline.
    let m = mat3x3f(
      vec3f( 1.2249, -0.2247,  0.0000),
      vec3f(-0.0420,  1.0419,  0.0000),
      vec3f(-0.0197, -0.0786,  1.0979),
    );
    return clamp(m * rgb, vec3f(0.0), vec3f(1.0));
  }
  return rgb;
}

fn ambientSample(ndc: vec2f) -> vec3f {
  // Same frame, "cover" projection, blurred via Poisson-disk taps so the
  // contain-mode bars show the dominant edge color of the video.
  let scale = computeUvScale(0u);
  let baseUv = vec2f(0.5) + (ndc - vec2f(0.5)) * scale;
  let r = 0.07;
  var offsets = array<vec2f, 13>(
    vec2f( 0.000,  0.000),
    vec2f( 0.535,  0.180),
    vec2f( 0.495, -0.366),
    vec2f( 0.220,  0.527),
    vec2f(-0.366, -0.220),
    vec2f(-0.527,  0.495),
    vec2f(-0.180,  0.535),
    vec2f( 0.366,  0.220),
    vec2f(-0.495,  0.366),
    vec2f(-0.220, -0.535),
    vec2f( 0.180, -0.495),
    vec2f(-0.535, -0.180),
    vec2f( 0.000,  0.700),
  );
  var col = vec3f(0.0);
  for (var i = 0u; i < 13u; i = i + 1u) {
    col = col + sampleTex(baseUv + offsets[i] * r);
  }
  return col / 13.0 * 0.78;
}

fn glassSample(ndc: vec2f) -> vec3f {
  let scale = computeUvScale(u.resizeMode);
  let uv = vec2f(0.5) + (ndc - vec2f(0.5)) * scale;
  var c = sampleTex(uv);
  c = applyShaderEffect(c, u.shaderEffect);
  c = applyColorSpace(c, u.colorSpace);
  return c;
}

fn liquidGlassButton(pixel: vec2f, center: vec2f, radius: f32, tint: vec3f) -> vec4f {
  let dv = pixel - center;
  let d = length(dv);
  if (d > radius + 1.5) {
    return vec4f(0.0);
  }
  let edgeT = clamp(1.0 - d / radius, 0.0, 1.0);
  let nrm = dv / max(d, 0.0001);
  // Bend more strongly near the rim, then taper to zero exactly at the rim.
  let refractStrength = pow(1.0 - edgeT, 1.8) * (1.0 - smoothstep(0.92, 1.0, d / radius)) * 22.0;
  let offset = -nrm * refractStrength / u.canvasSize;
  let baseNdc = pixel / u.canvasSize;
  let refracted = glassSample(baseNdc + offset);
  // Frosted glass: also pick up a slightly displaced 4-tap blur and mix.
  var frost = vec3f(0.0);
  let f = 4.0 / u.canvasSize;
  frost = frost + glassSample(baseNdc + vec2f( f.x,  f.y));
  frost = frost + glassSample(baseNdc + vec2f(-f.x,  f.y));
  frost = frost + glassSample(baseNdc + vec2f( f.x, -f.y));
  frost = frost + glassSample(baseNdc + vec2f(-f.x, -f.y));
  frost = frost * 0.25;
  var col = mix(refracted, frost, 0.35);
  // Tint and top-down specular.
  let topHi = smoothstep(0.55, 1.0, edgeT) * smoothstep(0.4, -0.6, nrm.y);
  let rim = smoothstep(0.96, 1.0, d / radius) * (1.0 - smoothstep(1.0, 1.05, d / radius));
  col = mix(col, tint, 0.12);
  col = col + vec3f(0.55) * topHi;
  col = col + vec3f(0.9) * rim * 0.6;
  let aa = 1.0 - smoothstep(radius - 1.0, radius + 0.5, d);
  return vec4f(col, aa);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let ndc = in.uv;
  let scale = computeUvScale(u.resizeMode);
  let uv = vec2f(0.5) + (ndc - vec2f(0.5)) * scale;
  let inside = uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;

  var color: vec3f;
  if (inside) {
    color = sampleTex(uv);
  } else {
    color = vec3f(0.0);
  }

  if (u.ambient == 1u && !inside) {
    color = ambientSample(ndc);
  }

  color = applyShaderEffect(color, u.shaderEffect);
  color = applyColorSpace(color, u.colorSpace);

  if (u.liquidGlass == 1u) {
    let pixel = ndc * u.canvasSize;
    let cx = u.canvasSize.x * 0.5;
    let cy = u.canvasSize.y * 0.78;
    let r = min(u.canvasSize.x, u.canvasSize.y) * 0.075;
    let sp = r * 2.7;
    let b1 = liquidGlassButton(pixel, vec2f(cx - sp, cy), r, vec3f(1.00, 0.55, 0.30));
    let b2 = liquidGlassButton(pixel, vec2f(cx,      cy), r * 1.15, vec3f(0.45, 0.85, 1.00));
    let b3 = liquidGlassButton(pixel, vec2f(cx + sp, cy), r, vec3f(1.00, 0.42, 0.65));
    color = mix(color, b1.rgb, b1.a);
    color = mix(color, b2.rgb, b2.a);
    color = mix(color, b3.rgb, b3.a);
  }

  return vec4f(color, 1.0);
}
`;

// rnwebgpu/shared-texture-memory is our umbrella that expands to the
// platform's shared-memory + shared-fence pair (the IOSurface / AHB still
// flows through SharedTextureMemory under the hood). Plus
// dawn-multi-planar-formats so Dawn can interpret the NV12 surface as a
// biplanar texture.
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/shared-texture-memory" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

const VIDEO_URL =
  "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_5MB.mp4";

const RESIZE_LABELS = ["Cover", "Contain", "Stretch", "Center"] as const;
const EFFECT_LABELS = ["Off", "Gray", "Sepia", "Invert", "Vibrant"] as const;
const COLOR_LABELS = ["Normal", "Warm", "Cool", "P3"] as const;
const TOGGLE_LABELS = ["Off", "On"] as const;

type Modes = {
  resize: number;
  effect: number;
  color: number;
  ambient: number;
  glass: number;
};

const INITIAL_MODES: Modes = {
  resize: 0,
  effect: 0,
  color: 0,
  ambient: 0,
  glass: 0,
};

export const ExternalTexture = () => {
  const ref = useCanvasRef();
  const [error, setError] = useState<string | null>(null);
  const rafRef = useRef<number | null>(null);

  const { device, adapter } = useDevice(undefined, {
    requiredFeatures: REQUIRED_FEATURES,
  });

  // Refs let the render loop pick up new mode values without re-running the
  // useEffect (which would tear down and rebuild the pipeline + player).
  const modesRef = useRef<Modes>(INITIAL_MODES);
  const [modes, setModes] = useState<Modes>(INITIAL_MODES);
  const cycle = (key: keyof Modes, max: number) => {
    const next = {
      ...modesRef.current,
      [key]: (modesRef.current[key] + 1) % max,
    };
    modesRef.current = next;
    setModes(next);
  };

  useEffect(() => {
    if (!device) {
      return;
    }
    const missing = REQUIRED_FEATURES.filter((f) => !device.features.has(f));
    if (missing.length > 0) {
      setError(
        `Device is missing required features [${missing.join(", ")}]. Adapter supports: ${
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

    // Same Big Buck Bunny URL as the SharedTextureMemory demo, but ask AVPlayer
    // for native NV12 instead of BGRA. Each VideoFrame now carries the YUV
    // matrix + plane info that importExternalTexture needs.
    const player = RNWebGPU.createVideoPlayer(VIDEO_URL, "nv12");
    player.play();

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

    // 48-byte uniform: vec2 texSize, vec2 canvasSize, 5 u32 mode flags,
    // f32 time, vec2 padding. Built on a single ArrayBuffer so we can pack
    // mixed f32/u32 fields in one writeBuffer call per frame.
    const uniformBuffer = device.createBuffer({
      size: 48,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
    const uniformData = new ArrayBuffer(48);
    const uniformF32 = new Float32Array(uniformData);
    const uniformU32 = new Uint32Array(uniformData);

    // The video plays at ~24fps but we tick at the display's 60Hz, so most rAF
    // ticks have no new frame from AVPlayer. Hold the latest VideoFrame across
    // ticks and re-import an ExternalTexture from it on the "no new frame"
    // ticks — this is what stops the canvas from flashing black ~2/3 of the
    // time. AVPlayer's pool is several buffers deep so holding one back like
    // this doesn't stall decoding.
    let currentFrame: VideoFrame | null = null;

    const startTime = performance.now();

    const render = () => {
      const newFrame = player.copyLatestFrame();
      if (newFrame) {
        if (currentFrame) {
          currentFrame.release();
        }
        currentFrame = newFrame;
      }

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

      if (currentFrame) {
        // GPUExternalTexture expires after each submit, so we rebuild one
        // every tick — even when sampling the same VideoFrame as last tick.
        let externalTex: GPUExternalTexture | null = null;
        try {
          externalTex = device.importExternalTexture({
            source: currentFrame,
            label: "video-external",
          });
        } catch (e) {
          console.warn("[ExternalTexture] importExternalTexture failed:", e);
        }

        if (externalTex) {
          const m = modesRef.current;
          uniformF32[0] = currentFrame.width;
          uniformF32[1] = currentFrame.height;
          uniformF32[2] = canvas.width;
          uniformF32[3] = canvas.height;
          uniformU32[4] = m.resize;
          uniformU32[5] = m.effect;
          uniformU32[6] = m.color;
          uniformU32[7] = m.ambient;
          uniformU32[8] = m.glass;
          uniformF32[9] = (performance.now() - startTime) / 1000;
          uniformF32[10] = 0;
          uniformF32[11] = 0;
          device.queue.writeBuffer(uniformBuffer, 0, uniformData);

          const bindGroup = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(0),
            entries: [
              { binding: 0, resource: externalTex },
              { binding: 1, resource: sampler },
              { binding: 2, resource: { buffer: uniformBuffer } },
            ],
          });
          pass.setPipeline(pipeline);
          pass.setBindGroup(0, bindGroup);
          pass.draw(3);
        }
      }

      pass.end();
      device.queue.submit([encoder.finish()]);
      context.present();
      rafRef.current = requestAnimationFrame(render);
    };
    rafRef.current = requestAnimationFrame(render);

    return () => {
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
      if (currentFrame) {
        currentFrame.release();
        currentFrame = null;
      }
      uniformBuffer.destroy();
      player.release();
    };
  }, [device, adapter, ref]);

  const buttons = useMemo(
    () => [
      {
        title: "Fit",
        value: RESIZE_LABELS[modes.resize],
        onPress: () => cycle("resize", RESIZE_LABELS.length),
      },
      {
        title: "Effect",
        value: EFFECT_LABELS[modes.effect],
        onPress: () => cycle("effect", EFFECT_LABELS.length),
      },
      {
        title: "Color",
        value: COLOR_LABELS[modes.color],
        onPress: () => cycle("color", COLOR_LABELS.length),
      },
      {
        title: "Ambient",
        value: TOGGLE_LABELS[modes.ambient],
        onPress: () => cycle("ambient", TOGGLE_LABELS.length),
      },
      {
        title: "Glass",
        value: TOGGLE_LABELS[modes.glass],
        onPress: () => cycle("glass", TOGGLE_LABELS.length),
      },
    ],
    [modes],
  );

  if (error) {
    return (
      <View style={styles.errorContainer}>
        <Text style={styles.errorText}>{error}</Text>
      </View>
    );
  }
  return (
    <View style={styles.root}>
      <Canvas ref={ref} style={styles.canvas} />
      <View style={styles.toolbar} pointerEvents="box-none">
        <ScrollView
          horizontal
          showsHorizontalScrollIndicator={false}
          contentContainerStyle={styles.toolbarContent}
        >
          {buttons.map((b) => (
            <Pressable
              key={b.title}
              onPress={b.onPress}
              style={({ pressed }) => [
                styles.button,
                pressed && styles.buttonPressed,
              ]}
            >
              <Text style={styles.buttonTitle}>{b.title}</Text>
              <Text style={styles.buttonValue}>{b.value}</Text>
            </Pressable>
          ))}
        </ScrollView>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  canvas: { flex: 1 },
  errorContainer: { flex: 1, padding: 16, justifyContent: "center" },
  errorText: { color: "red", fontSize: 14 },
  toolbar: {
    position: "absolute",
    left: 0,
    right: 0,
    top: 12,
  },
  toolbarContent: {
    paddingHorizontal: 12,
    gap: 8,
  },
  button: {
    backgroundColor: "rgba(0,0,0,0.55)",
    borderColor: "rgba(255,255,255,0.18)",
    borderWidth: 1,
    borderRadius: 14,
    paddingHorizontal: 12,
    paddingVertical: 8,
    minWidth: 84,
  },
  buttonPressed: {
    backgroundColor: "rgba(255,255,255,0.18)",
  },
  buttonTitle: {
    color: "rgba(255,255,255,0.65)",
    fontSize: 11,
    fontWeight: "500",
    letterSpacing: 0.4,
    textTransform: "uppercase",
  },
  buttonValue: {
    color: "white",
    fontSize: 15,
    fontWeight: "600",
    marginTop: 2,
  },
});
