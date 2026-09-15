// Shared WGSL helper: maps a UV in the *upright* image (what the user sees,
// what the detector sees) back to a UV in the raw camera texture.
//
// Vision Camera delivers the sensor's native buffer untouched and tells us
// how it relates to the upright picture via frame.orientation (0 up,
// 1 right, 2 down, 3 left, matching CameraOrientation) and frame.isMirrored.
// Mirroring is applied in upright space so a front camera reads like a
// selfie mirror (left/right swap as seen on screen), then the rotation
// undoes the sensor orientation. The inverse rotations match Vision
// Camera's own FrameCoordinateSystemConverter.
const ORIENT_WGSL = /* wgsl */ `
fn toTexUV(p0: vec2f, rotation: u32, mirror: u32) -> vec2f {
  var p = p0;
  // The imported camera surface reads bottom-up relative to the metadata
  // above (verified on iPhone with Vision Camera 5), so flip vertically in
  // upright space before undoing the sensor rotation.
  p.y = 1.0 - p.y;
  if (mirror == 1u) {
    p.x = 1.0 - p.x;
  }
  switch (rotation) {
    case 1u: { return vec2f(1.0 - p.y, p.x); }
    case 2u: { return vec2f(1.0 - p.x, 1.0 - p.y); }
    case 3u: { return vec2f(p.y, 1.0 - p.x); }
    default: { return p; }
  }
}

// Size of the frame once rotated upright.
fn uprightSize(texSize: vec2f, rotation: u32) -> vec2f {
  if (rotation == 1u || rotation == 3u) {
    return vec2f(texSize.y, texSize.x);
  }
  return texSize;
}
`;

// Display shader: samples the camera as an external texture (hardware
// YUV→RGB + sRGB conversion via textureSampleBaseClampToEdge), then
// composites per-face glowing rings on top. Face boxes live in a small
// uniform sized for up to 8 faces; drawing the overlay is just SDFs over
// UVs, no pixel roundtripping.
export const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

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
// sub-rectangle of the upright image matching the canvas aspect ratio.
fn coverScale(imageSize: vec2f) -> vec2f {
  let canvasAR = u.canvasSize.x / u.canvasSize.y;
  let imageAR = imageSize.x / imageSize.y;
  if (imageAR > canvasAR) {
    return vec2f(canvasAR / imageAR, 1.0);
  }
  return vec2f(1.0, imageAR / canvasAR);
}

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
  let scale = coverScale(imageSize);
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

// Tiny pipeline that blits the upright camera image into an offscreen rgba8
// texture so the face detector can read pixels. Stretches the full image
// across the target; the face boxes we get back are in normalized upright
// UV space, which lines up with the display shader's ring math.
export const DETECT_SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};
struct Uniforms {
  rotation: u32,
  mirror: u32,
  _pad0: u32,
  _pad1: u32,
};
@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

${ORIENT_WGSL}

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
  let uvTex = toTexUV(in.uv, u.rotation, u.mirror);
  return textureSampleBaseClampToEdge(srcTex, srcSampler, uvTex);
}
`;
