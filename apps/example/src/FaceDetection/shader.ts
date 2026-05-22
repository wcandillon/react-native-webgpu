// Display shader: samples the video as an external texture (hardware YUV→RGB
// + sRGB conversion via textureSampleBaseClampToEdge), then composites
// per-face glowing rings on top. Face boxes live in a small uniform sized
// for up to 8 faces — drawing the overlay is just SDFs over UVs, no pixel
// roundtripping.
export const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

// Uniform layout (160 bytes):
//   0  : vec2f  texSize
//   8  : vec2f  canvasSize
//  16  : u32    numFaces
//  20  : f32    time (seconds)
//  24  : f32    _pad0
//  28  : f32    _pad1   (16-byte align before array)
//  32  : array<vec4f, 8> faces   // (xMin, yMin, width, height) in 0..1
struct Uniforms {
  texSize: vec2f,
  canvasSize: vec2f,
  numFaces: u32,
  time: f32,
  _pad0: f32,
  _pad1: f32,
  faces: array<vec4f, 8>,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
  // Full-screen triangle.
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

// 'cover' fit: scale UVs around (0.5, 0.5) so the canvas samples a
// sub-rectangle of the texture matching the canvas aspect ratio.
fn coverScale() -> vec2f {
  let canvasAR = u.canvasSize.x / u.canvasSize.y;
  let texAR = u.texSize.x / u.texSize.y;
  if (texAR > canvasAR) {
    return vec2f(canvasAR / texAR, 1.0);
  }
  return vec2f(1.0, texAR / canvasAR);
}

// Pulsing ring in the same UV space as the video texture. We work in
// pixel-equivalent units (scaled by texSize) so the ring thickness stays
// visually constant regardless of face size.
fn ringIntensity(uvTex: vec2f, box: vec4f, t: f32) -> f32 {
  let center = box.xy + box.zw * 0.5;
  let pixel = uvTex * u.texSize;
  let centerPx = center * u.texSize;
  let radiusPx = max(box.z * u.texSize.x, box.w * u.texSize.y) * 0.65;
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
  let ndc = in.uv;
  let scale = coverScale();
  let uvTex = vec2f(0.5) + (ndc - vec2f(0.5)) * scale;
  let inside = uvTex.x >= 0.0 && uvTex.x <= 1.0 &&
               uvTex.y >= 0.0 && uvTex.y <= 1.0;

  var color: vec3f;
  if (inside) {
    color = textureSampleBaseClampToEdge(srcTex, srcSampler, uvTex).rgb;
  } else {
    color = vec3f(0.0);
  }

  var ringAccum = 0.0;
  let n = min(u.numFaces, 8u);
  for (var i = 0u; i < n; i = i + 1u) {
    ringAccum = ringAccum + ringIntensity(uvTex, u.faces[i], u.time);
  }
  ringAccum = clamp(ringAccum, 0.0, 1.0);

  // Cyan ring on a slightly dimmed background so the glow reads.
  let ringColor = vec3f(0.35, 0.85, 1.0);
  color = mix(color, color * 0.55 + ringColor, ringAccum);

  return vec4f(color, 1.0);
}
`;

// Tiny pipeline that just blits the video into an offscreen rgba8 texture so
// the face detector can read pixels. Stretches the full texture across the
// target — the face boxes we get back are already in normalized texture UV
// space, which lines up with the display shader's sampling.
export const DETECT_SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};
@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
  var p = array<vec2f, 3>(vec2f(-1.0,-3.0), vec2f(-1.0, 1.0), vec2f( 3.0, 1.0));
  var uv = array<vec2f, 3>(vec2f( 0.0, 2.0), vec2f( 0.0, 0.0), vec2f( 2.0, 0.0));
  var o: VsOut;
  o.position = vec4f(p[vid], 0.0, 1.0);
  o.uv = uv[vid];
  return o;
}
@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  return textureSampleBaseClampToEdge(srcTex, srcSampler, in.uv);
}
`;
