export const fullscreenTriangleVertWGSL = /* wgsl */ `
struct VSOut {
  @builtin(position) position : vec4<f32>,
  @location(0) uv : vec2<f32>,
};

@vertex
fn main(@builtin(vertex_index) idx : u32) -> VSOut {
  var pos = array<vec2<f32>, 3>(
    vec2<f32>(-1.0, -1.0),
    vec2<f32>( 3.0, -1.0),
    vec2<f32>(-1.0,  3.0),
  );
  let p = pos[idx];
  var out : VSOut;
  out.position = vec4<f32>(p, 0.0, 1.0);
  out.uv = (p + vec2<f32>(1.0, 1.0)) * 0.5;
  out.uv.y = 1.0 - out.uv.y;
  return out;
}
`;

// Renders three vertical bands:
//   left third:   solid value 1.0 (SDR reference white)
//   middle third: black gap
//   right third:  solid value = peak (HDR if peak > 1)
//
// With "extended" tone mapping on an EDR-capable display the right band
// glows visibly brighter than the left. With "standard" tone mapping
// the right band is clamped to 1.0 and matches the left.
export const hdrBandFragWGSL = /* wgsl */ `
struct Params {
  peak : f32,
  _p0 : f32,
  _p1 : f32,
  _p2 : f32,
};

@group(0) @binding(0) var<uniform> params : Params;

@fragment
fn main(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> {
  let x = uv.x;
  if (x < 0.4) {
    // SDR reference white.
    return vec4<f32>(1.0, 1.0, 1.0, 1.0);
  } else if (x < 0.6) {
    // Black gap so the two whites are not adjacent.
    return vec4<f32>(0.0, 0.0, 0.0, 1.0);
  } else {
    // Bright (potentially HDR) white.
    return vec4<f32>(params.peak, params.peak, params.peak, 1.0);
  }
}
`;
