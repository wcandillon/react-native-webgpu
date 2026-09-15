import {
  FULLSCREEN_TRIANGLE_WGSL,
  ORIENT_WGSL,
} from "../VisionCamera/orientation";

// Display shader: samples the camera as an external texture (hardware
// YUV→RGB + sRGB conversion via textureSampleBaseClampToEdge), then
// composites per-face glowing rings on top. Face boxes live in a small
// uniform sized for up to 8 faces; drawing the overlay is just SDFs over
// UVs, no pixel roundtripping.
export const SHADER = /* wgsl */ `
// Uniform layout (160 bytes):
//   0  : vec2f  texSize      raw camera texture size
//   8  : vec2f  canvasSize
//  16  : u32    numFaces
//  20  : f32    time (seconds)
//  24  : u32    rotation     frame.orientation (0 up, 1 right, 2 down, 3 left)
//  28  : u32    mirror       frame.isMirrored
//  32  : array<vec4f, 8> faces   // (xMin, yMin, width, height) in upright 0..1
struct Uniforms {
  texSize: vec2f,
  canvasSize: vec2f,
  numFaces: u32,
  time: f32,
  rotation: u32,
  mirror: u32,
  faces: array<vec4f, 8>,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

${ORIENT_WGSL}
${FULLSCREEN_TRIANGLE_WGSL}

// Pulsing ring in upright image UV space. We work in pixel-equivalent
// units (scaled by imageSize) so the ring thickness stays visually constant
// regardless of face size.
fn ringIntensity(uvUp: vec2f, imageSize: vec2f, box: vec4f, t: f32) -> f32 {
  let center = box.xy + box.zw * 0.5;
  let pixel = uvUp * imageSize;
  let centerPx = center * imageSize;
  let radiusPx = max(box.z * imageSize.x, box.w * imageSize.y) * 0.65;
  let breath = 0.92 + 0.08 * sin(t * 5.0);
  let r = radiusPx * breath;
  let d = distance(pixel, centerPx);
  let ringWidth = max(radiusPx * 0.05, 2.0);
  let inner = smoothstep(r - ringWidth, r - ringWidth * 0.4, d);
  let outer = 1.0 - smoothstep(r + ringWidth * 0.4, r + ringWidth, d);
  let ring = inner * outer;
  // Soft outer glow falls off over a few ring widths.
  let falloff = ringWidth * 5.0;
  let glow = exp(-((d - r) * (d - r)) / (falloff * falloff)) * 0.45;
  return clamp(ring + glow, 0.0, 1.0);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let imageSize = uprightSize(u.texSize, u.rotation);
  let scale = coverScale(u.canvasSize, imageSize);
  let uvUp = vec2f(0.5) + (in.uv - vec2f(0.5)) * scale;
  let inside = uvUp.x >= 0.0 && uvUp.x <= 1.0 &&
               uvUp.y >= 0.0 && uvUp.y <= 1.0;

  var color: vec3f;
  if (inside) {
    let uvTex = toTexUV(uvUp, u.rotation, u.mirror);
    color = textureSampleBaseClampToEdge(srcTex, srcSampler, uvTex).rgb;
  } else {
    color = vec3f(0.0);
  }

  var ringAccum = 0.0;
  let n = min(u.numFaces, 8u);
  for (var i = 0u; i < n; i = i + 1u) {
    ringAccum = ringAccum + ringIntensity(uvUp, imageSize, u.faces[i], u.time);
  }
  ringAccum = clamp(ringAccum, 0.0, 1.0);

  // Cyan ring on a slightly dimmed background so the glow reads.
  let ringColor = vec3f(0.35, 0.85, 1.0);
  color = mix(color, color * 0.55 + ringColor, ringAccum);

  return vec4f(color, 1.0);
}
`;
