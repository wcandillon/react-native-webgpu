// "Copy a camera frame into a linear rgba16float texture" shader, shared by
// the sharp and the blurred env targets of CameraHelmet.
//
// The output texture is portrait and upright. The fragment shader maps each
// output uv back into the source frame using the orientation / mirror flags
// Vision Camera attaches to every Frame, decodes the ISP's sRGB output to
// linear light and stretches the highlights back out into an HDR range.
// Three.js then samples the result through a THREE.ExternalTexture on a
// plane that is sized from the camera's real field of view.

export const CAMERA_ENV_UNIFORM_SIZE = 32;

export const CAMERA_ENV_SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  // x: Frame.orientation (0 up, 1 right, 2 down, 3 left)
  // y: Frame.isMirrored (0 / 1)
  // z: box blur taps per axis (1 = plain copy, n = n*n taps)
  // w: unused
  modes: vec4u,
  // x: highlight boost (0 = keep the frame LDR)
  // y: blur radius in source uv units
  // z, w: unused
  params: vec4f,
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

// Output uv (portrait, upright) -> source frame uv.
//
// Landscape sensors deliver frames whose pixel data is rotated relative to
// the portrait output. "right" is the mapping that was tuned on device for
// the earlier version of this demo: a 90 degree turn with V flipped, i.e.
// (v, 1-u). The other cases are derived from it by quarter turns. A
// mirrored frame (front camera in selfie mode) is undone by flipping the
// source X after the rotation, so the texture ends up un-mirrored: the
// helmet then mirrors you the way a real chrome surface would.
fn sourceUv(uv: vec2f, orientation: u32, mirrored: u32) -> vec2f {
  var s: vec2f;
  if (orientation == 0u) {
    s = vec2f(uv.x, 1.0 - uv.y);
  } else if (orientation == 1u) {
    s = vec2f(uv.y, 1.0 - uv.x);
  } else if (orientation == 2u) {
    s = vec2f(1.0 - uv.x, uv.y);
  } else {
    s = vec2f(1.0 - uv.y, uv.x);
  }
  if (mirrored == 1u) {
    s.x = 1.0 - s.x;
  }
  return s;
}

fn srgbToLinear(c: vec3f) -> vec3f {
  let lo = c / 12.92;
  let hi = pow((c + vec3f(0.055)) / 1.055, vec3f(2.4));
  return select(lo, hi, c > vec3f(0.04045));
}

// The ISP clips everything brighter than paper white to 1.0, which is why a
// camera frame used as lighting never produces a real specular hotspot.
// Stretch the top of the range back out: pixels near white get several
// times brighter, midtones barely move. Not a measurement, but it gives
// windows and lamps the energy the metal needs to sparkle.
fn expandHighlights(lin: vec3f, boost: f32) -> vec3f {
  let l = dot(lin, vec3f(0.2126, 0.7152, 0.0722));
  let w = smoothstep(0.55, 1.0, l);
  return lin * (1.0 + boost * w * w);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let orientation = u.modes.x;
  let mirrored = u.modes.y;
  let taps = max(u.modes.z, 1u);
  let boost = u.params.x;
  let radius = u.params.y;

  var sum = vec3f(0.0);
  // Uniform loop bounds keep the texture_external sample in uniform control
  // flow. For the sharp copy taps == 1 and this collapses to one sample.
  for (var j = 0u; j < taps; j = j + 1u) {
    for (var i = 0u; i < taps; i = i + 1u) {
      var offset = vec2f(0.0);
      if (taps > 1u) {
        let t = vec2f(f32(i), f32(j)) / f32(taps - 1u) - vec2f(0.5);
        offset = t * 2.0 * radius;
      }
      let uv = clamp(in.uv + offset, vec2f(0.0), vec2f(1.0));
      let c = textureSampleBaseClampToEdge(
        srcTex, srcSampler, sourceUv(uv, orientation, mirrored));
      sum = sum + srgbToLinear(c.rgb);
    }
  }
  let lin = sum / f32(taps * taps);
  return vec4f(expandHighlights(lin, boost), 1.0);
}
`;
