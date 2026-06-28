// First-party hero shader (compute contract): a domain-warped fbm flow field.
// Demonstrates the compute.toys-style path: a @compute entry point that writes
// into the injected `screen` storage texture. Theme-aware so it reads well on
// both the light and dark home page.

import type { ShaderEntry } from "../types";

const CODE = /* wgsl */ `
fn hash2(p: vec2f) -> f32 {
  let h = dot(p, vec2f(127.1, 311.7));
  return fract(sin(h) * 43758.5453123);
}

fn noise(p: vec2f) -> f32 {
  let i = floor(p);
  let f = fract(p);
  let a = hash2(i);
  let b = hash2(i + vec2f(1.0, 0.0));
  let c = hash2(i + vec2f(0.0, 1.0));
  let d = hash2(i + vec2f(1.0, 1.0));
  let w = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, w.x), mix(c, d, w.x), w.y);
}

fn fbm(p: vec2f) -> f32 {
  var sum = 0.0;
  var amp = 0.5;
  var freq = p;
  for (var i = 0; i < 5; i++) {
    sum += amp * noise(freq);
    freq *= 2.02;
    amp *= 0.5;
  }
  return sum;
}

// Cosine palette (Inigo Quilez style).
fn palette(t: f32) -> vec3f {
  let a = vec3f(0.5, 0.5, 0.5);
  let b = vec3f(0.5, 0.5, 0.5);
  let c = vec3f(1.0, 1.0, 1.0);
  let d = vec3f(0.0, 0.10, 0.20);
  return a + b * cos(6.28318 * (c * t + d));
}

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) gid: vec3u) {
  let dims = vec2u(u.resolution);
  if (gid.x >= dims.x || gid.y >= dims.y) {
    return;
  }

  let res = u.resolution;
  var uv = (vec2f(gid.xy) - 0.5 * res) / res.y;
  let t = u.time * 0.15;

  // Domain warp.
  let q = vec2f(fbm(uv * 2.5 + vec2f(t, -t)), fbm(uv * 2.5 + vec2f(5.2, 1.3) - t));
  let r = vec2f(
    fbm(uv * 2.5 + 3.0 * q + vec2f(1.7, 9.2) + 0.15 * t),
    fbm(uv * 2.5 + 3.0 * q + vec2f(8.3, 2.8) - 0.12 * t),
  );
  let f = fbm(uv * 2.5 + 4.0 * r);

  var col = palette(f + t * 0.5);
  col = mix(col, vec3f(dot(col, vec3f(0.299, 0.587, 0.114))), 0.35);

  // Theme-aware: vivid-but-dim on dark, soft pastel on light.
  let dark = col * (0.18 + 0.55 * f);
  let light = mix(vec3f(0.95), col, 0.35) * (0.85 + 0.15 * f);
  let outCol = mix(dark, light, u.theme);

  let vignette = smoothstep(1.4, 0.2, length(uv));
  textureStore(screen, vec2i(gid.xy), vec4f(outCol * vignette, 1.0));
}
`;

export const flowField: ShaderEntry = {
  id: "flow-field",
  title: "Flow Field",
  author: "William Candillon",
  sourceUrl: "https://github.com/wcandillon/react-native-webgpu",
  license: "MIT",
  shader: {
    kind: "compute",
    code: CODE,
    workgroupSize: [8, 8],
  },
};
