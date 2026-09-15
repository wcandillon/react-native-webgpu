import {
  FULLSCREEN_TRIANGLE_WGSL,
  ORIENT_WGSL,
} from "../VisionCamera/orientation";

// Display shader: the live camera picture with a HUD-style corner bracket
// around every detected object. Boxes come in as upright-image UVs and are
// converted to canvas pixels so the bracket thickness stays constant on
// screen. Dogs get a warm gold, people cyan, cats magenta, the rest white.
export const SHADER = /* wgsl */ `
struct Box {
  // (xMin, yMin, width, height) in upright 0..1
  rect: vec4f,
  // x: kind (0 other, 1 dog, 2 person, 3 cat), y: score
  info: vec4f,
};

// Uniform layout: 32-byte header + 32 bytes per box.
struct Uniforms {
  texSize: vec2f,
  canvasSize: vec2f,
  numBoxes: u32,
  time: f32,
  rotation: u32,
  mirror: u32,
  boxes: array<Box, 10>,
};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;

${ORIENT_WGSL}
${FULLSCREEN_TRIANGLE_WGSL}

fn kindColor(kind: f32) -> vec3f {
  if (kind == 1.0) {
    return vec3f(1.0, 0.80, 0.20);
  }
  if (kind == 2.0) {
    return vec3f(0.35, 0.85, 1.0);
  }
  if (kind == 3.0) {
    return vec3f(1.0, 0.40, 0.87);
  }
  return vec3f(0.95, 0.95, 0.95);
}

// Signed distance to an axis-aligned box (negative inside).
fn sdBox(p: vec2f, half: vec2f) -> f32 {
  let d = abs(p) - half;
  return length(max(d, vec2f(0.0))) + min(max(d.x, d.y), 0.0);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let imageSize = uprightSize(u.texSize, u.rotation);
  let scale = coverScale(u.canvasSize, imageSize);
  let uvUp = vec2f(0.5) + (in.uv - vec2f(0.5)) * scale;
  let inside = all(uvUp >= vec2f(0.0)) && all(uvUp <= vec2f(1.0));
  let uvTex = toTexUV(clamp(uvUp, vec2f(0.0), vec2f(1.0)), u.rotation, u.mirror);
  var color = textureSampleBaseClampToEdge(srcTex, srcSampler, uvTex).rgb;
  color = select(vec3f(0.0), color, inside);

  let px = in.uv * u.canvasSize;
  let thick = max(3.0, u.canvasSize.x * 0.004);
  let n = min(u.numBoxes, 10u);
  for (var i = 0u; i < n; i = i + 1u) {
    let b = u.boxes[i];
    // Upright UV rect -> canvas pixels.
    let minUV = vec2f(0.5) + (b.rect.xy - vec2f(0.5)) / scale;
    let maxUV = vec2f(0.5) + (b.rect.xy + b.rect.zw - vec2f(0.5)) / scale;
    let minPx = minUV * u.canvasSize;
    let maxPx = maxUV * u.canvasSize;
    let center = (minPx + maxPx) * 0.5;
    let half = (maxPx - minPx) * 0.5;
    let sd = sdBox(px - center, half);

    // Outline, kept only near the corners so it reads as brackets.
    let edge = 1.0 - smoothstep(thick - 1.0, thick + 1.0, abs(sd));
    let bracketLen = min(half.x, half.y) * 0.35 + thick;
    let q = abs(px - center);
    let nearCorner = clamp(
      step(half.x - bracketLen, q.x) + step(half.y - bracketLen, q.y),
      0.0, 1.0,
    );
    let bracket = edge * nearCorner;

    // Faint fill inside the box, stronger for dogs.
    let isDog = select(0.0, 1.0, b.info.x == 1.0);
    let fill = (1.0 - step(0.0, sd)) * (0.06 + 0.10 * isDog);

    let pulse = 0.85 + 0.15 * sin(u.time * 4.0 + f32(i));
    let a = clamp(bracket * pulse + fill, 0.0, 1.0);
    color = mix(color, kindColor(b.info.x), a);
  }

  return vec4f(color, 1.0);
}
`;
