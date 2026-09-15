// Shared helpers for turning a raw Vision Camera frame into the upright
// picture the user expects to see.
//
// Vision Camera delivers the sensor's native buffer untouched and tells us
// how it relates to the upright image via frame.orientation and
// frame.isMirrored. Every demo built on useCameraInference draws in
// "upright UV" space (0..1 across the picture as seen on screen) and maps
// back to the raw texture with toTexUV when sampling.

// frame.orientation -> the rotation index the shaders expect.
export const ORIENTATION_INDEX = { up: 0, right: 1, down: 2, left: 3 } as const;

// WGSL: upright UV -> raw camera texture UV, plus the upright frame size.
// The rotation undoes the sensor orientation first, then frame.isMirrored
// flips the *raw* texture's x axis. This is the inverse of Vision Camera's
// own FrameCoordinateSystemConverter (rotate, then mirror in frame space)
// and holds for both the mirrored front camera and the plain back camera.
export const ORIENT_WGSL = /* wgsl */ `
fn toTexUV(p: vec2f, rotation: u32, mirror: u32) -> vec2f {
  var t: vec2f;
  switch (rotation) {
    case 1u: { t = vec2f(1.0 - p.y, p.x); }
    case 2u: { t = vec2f(1.0 - p.x, 1.0 - p.y); }
    case 3u: { t = vec2f(p.y, 1.0 - p.x); }
    default: { t = p; }
  }
  if (mirror == 1u) {
    t.x = 1.0 - t.x;
  }
  return t;
}

// Size of the frame once rotated upright.
fn uprightSize(texSize: vec2f, rotation: u32) -> vec2f {
  if (rotation == 1u || rotation == 3u) {
    return vec2f(texSize.y, texSize.x);
  }
  return texSize;
}

// 'cover' fit: scale UVs around (0.5, 0.5) so a canvas samples the
// sub-rectangle of the upright image that matches its aspect ratio.
fn coverScale(canvasSize: vec2f, imageSize: vec2f) -> vec2f {
  let canvasAR = canvasSize.x / canvasSize.y;
  let imageAR = imageSize.x / imageSize.y;
  if (imageAR > canvasAR) {
    return vec2f(canvasAR / imageAR, 1.0);
  }
  return vec2f(1.0, imageAR / canvasAR);
}
`;

// WGSL: full-screen triangle vertex stage. uv runs 0..1 top-left to
// bottom-right across the render target.
export const FULLSCREEN_TRIANGLE_WGSL = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

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
`;

// Blits the upright camera image across the whole render target. Used for
// the model input texture and by demos that want an upright rgba8 copy of
// the frame (three.js overlays, for instance).
//
// The vertical flip is empirical: with the same vertex stage, a pass into an
// off-screen texture comes out upside down relative to a pass into the
// canvas on iOS (readbacks and three.js both showed inverted frames while
// the on-screen shaders were upright). Flipping here keeps every
// off-screen copy in the same orientation as the canvas.
export const CAMERA_BLIT_SHADER = /* wgsl */ `
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
${FULLSCREEN_TRIANGLE_WGSL}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let uvTex = toTexUV(vec2f(in.uv.x, 1.0 - in.uv.y), u.rotation, u.mirror);
  return textureSampleBaseClampToEdge(srcTex, srcSampler, uvTex);
}
`;

// JS twin of the WGSL coverScale, for laying out native overlays (labels,
// three.js geometry) in the same space as the shaders.
export const coverScale = (
  canvasWidth: number,
  canvasHeight: number,
  imageWidth: number,
  imageHeight: number,
): [number, number] => {
  const canvasAR = canvasWidth / canvasHeight;
  const imageAR = imageWidth / imageHeight;
  if (imageAR > canvasAR) {
    return [canvasAR / imageAR, 1];
  }
  return [1, imageAR / canvasAR];
};

// Upright UV -> canvas UV (0..1 across the canvas) for a cover-fit picture.
export const uprightToCanvas = (
  u: number,
  v: number,
  scale: [number, number],
): [number, number] => [0.5 + (u - 0.5) / scale[0], 0.5 + (v - 0.5) / scale[1]];
