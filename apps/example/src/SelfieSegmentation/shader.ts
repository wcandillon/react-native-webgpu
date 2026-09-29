import {
  FULLSCREEN_TRIANGLE_WGSL,
  ORIENT_WGSL,
} from "../VisionCamera/orientation";

// Display shader: the person mask decides, per pixel, between the live
// camera and a background treatment computed right here in WGSL. The mask
// is the segmentation model's output buffer, copied on the GPU queue with no
// CPU round trip: an [h, w, 4] float layout with the person probability in
// the first channel, sampled here with manual bilinear filtering.
// Three modes:
//   0  blur       12-tap disc blur of the camera behind the person
//   1  replace    animated gradient backdrop
//   2  spotlight  darkened, desaturated background
export const makeShader = (maskSize: number) => /* wgsl */ `
struct Uniforms {
  texSize: vec2f,
  canvasSize: vec2f,
  mode: u32,
  time: f32,
  rotation: u32,
  mirror: u32,
};

const MASK_SIZE: i32 = ${maskSize};

@group(0) @binding(0) var srcTex: texture_external;
@group(0) @binding(1) var srcSampler: sampler;
@group(0) @binding(2) var<uniform> u: Uniforms;
@group(0) @binding(3) var<storage, read> mask: array<f32>;

${ORIENT_WGSL}
${FULLSCREEN_TRIANGLE_WGSL}

fn sampleCamera(uvUp: vec2f) -> vec3f {
  let uvTex = toTexUV(clamp(uvUp, vec2f(0.0), vec2f(1.0)), u.rotation, u.mirror);
  return textureSampleBaseClampToEdge(srcTex, srcSampler, uvTex).rgb;
}

fn maskTexel(x: i32, y: i32) -> f32 {
  let cx = clamp(x, 0, MASK_SIZE - 1);
  let cy = clamp(y, 0, MASK_SIZE - 1);
  return mask[u32(cy * MASK_SIZE + cx) * 4u];
}

// Bilinear lookup of the person probability at an upright UV.
fn personAt(uvUp: vec2f) -> f32 {
  let p = uvUp * f32(MASK_SIZE) - vec2f(0.5);
  let i = vec2i(floor(p));
  let f = fract(p);
  let a = maskTexel(i.x, i.y);
  let b = maskTexel(i.x + 1, i.y);
  let c = maskTexel(i.x, i.y + 1);
  let d = maskTexel(i.x + 1, i.y + 1);
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Disc blur in upright space. The x offset is divided by the image aspect
// so the disc stays round on screen.
fn blurred(uvUp: vec2f, imageSize: vec2f) -> vec3f {
  let radius = 0.02;
  let aspect = imageSize.x / imageSize.y;
  var sum = sampleCamera(uvUp);
  for (var i = 0u; i < 12u; i = i + 1u) {
    let angle = f32(i) * 0.5236;
    let r = radius * select(0.55, 1.0, (i % 2u) == 0u);
    let offset = vec2f(cos(angle) * r / aspect, sin(angle) * r);
    sum = sum + sampleCamera(uvUp + offset);
  }
  return sum / 13.0;
}

fn gradient(uvUp: vec2f, t: f32) -> vec3f {
  let wave = 0.12 * sin(uvUp.x * 5.0 + t * 0.8) + 0.08 * cos(uvUp.y * 7.0 - t * 0.6);
  let k = clamp(uvUp.y + wave, 0.0, 1.0);
  let top = vec3f(0.08, 0.05, 0.35);
  let bottom = vec3f(0.95, 0.35, 0.55);
  var c = mix(top, bottom, k);
  // Soft diagonal light bands to give the backdrop some life.
  let band = 0.5 + 0.5 * sin((uvUp.x - uvUp.y) * 18.0 + t * 1.5);
  c = c + vec3f(0.06) * band;
  return c;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let imageSize = uprightSize(u.texSize, u.rotation);
  let scale = coverScale(u.canvasSize, imageSize);
  let uvUp = vec2f(0.5) + (in.uv - vec2f(0.5)) * scale;
  let inside = all(uvUp >= vec2f(0.0)) && all(uvUp <= vec2f(1.0));

  let m = smoothstep(0.3, 0.7, personAt(uvUp));
  let color = sampleCamera(uvUp);

  var background: vec3f;
  if (u.mode == 0u) {
    background = blurred(uvUp, imageSize);
  } else if (u.mode == 1u) {
    background = gradient(uvUp, u.time);
  } else {
    let luma = dot(color, vec3f(0.299, 0.587, 0.114));
    background = vec3f(luma) * 0.3;
  }

  var result = mix(background, color, m);
  result = select(vec3f(0.0), result, inside);
  return vec4f(result, 1.0);
}
`;
