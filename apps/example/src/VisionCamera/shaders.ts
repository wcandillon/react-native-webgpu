import { Platform } from "react-native";

// Main camera-effects shader: samples the imported external texture (camera
// frame) with cover-fit + optional chromatic aberration / pixelate, optionally
// mixes in the pre-blurred backdrop, then applies effect / tint / vignette.
// `cameraCoord` / `cameraDecode` come from CAMERA_PRELUDE, which is prepended
// at shader-module creation time.
export const SHADER = /* wgsl */ `
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
  return cameraDecode(
    textureSampleBaseClampToEdge(srcTex, srcSampler, cameraCoord(snap(uv, block))),
  );
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

// Android delivers camera frames as an external-format (opaque) YUV buffer.
// Dawn's OpaqueYCbCrAndroidForExternalTexture path samples it through a Vulkan
// YCbCr conversion that is hard-coded to RGB_IDENTITY (Dawn's
// SamplerVk.cpp::GetYCbCrForTextureView; see crbug.com/497675620), so the
// external sample comes back as raw [Y, Cb, Cr] on *every* device — this is by
// design, not a driver quirk — and we do the BT.709 YUV->RGB ourselves below.
// (Dawn's own SharedTextureMemoryOpaqueYCbCrAndroidForExternalTexture
// .NoopSampleY8Cb8Cr8AHB test asserts the same raw passthrough.) The frame also
// comes out mirrored on both axes relative to the canvas (Android buffer
// origin), so we flip X and Y. iOS goes through the native two-plane path,
// which already converts and orients, so this correction is Android-only. The
// prelude is prepended to every shader module that samples the camera (main
// pass + blur prepass).
export const CAMERA_PRELUDE = /* wgsl */ `
const CAMERA_IS_YUV: bool = ${Platform.OS === "android"};
const CAMERA_FLIP_X: bool = ${Platform.OS === "android"};
const CAMERA_FLIP_Y: bool = ${Platform.OS === "android"};

fn cameraCoord(uv: vec2f) -> vec2f {
  var c = uv;
  if (CAMERA_FLIP_X) {
    c.x = 1.0 - c.x;
  }
  if (CAMERA_FLIP_Y) {
    c.y = 1.0 - c.y;
  }
  return c;
}

// BT.709 limited-range YUV -> RGB. On the Android opaque path the sampled
// channels are always raw [Y, Cb, Cr] (Dawn forces an RGB_IDENTITY Vulkan
// conversion); a no-op passthrough on every other platform.
fn cameraDecode(c: vec4f) -> vec4f {
  if (!CAMERA_IS_YUV) {
    return c;
  }
  let y = c.r - 0.0627451;
  let cb = c.g - 0.5;
  let cr = c.b - 0.5;
  let r = 1.164384 * y + 1.792741 * cr;
  let g = 1.164384 * y - 0.213249 * cb - 0.532909 * cr;
  let b = 1.164384 * y + 2.112402 * cb;
  return vec4f(clamp(vec3f(r, g, b), vec3f(0.0), vec3f(1.0)), 1.0);
}
`;
