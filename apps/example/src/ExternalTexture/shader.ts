// importExternalTexture is the spec-mandated path for "I have a YUV-encoded
// video/camera frame and I want to sample it in a shader without copying or
// hand-rolling YUV math". The WGSL side uses texture_external +
// textureSampleBaseClampToEdge; the driver does the planar fetch, YUV→RGB
// matrix multiply, sRGB transfer, and gamut conversion in the sampler.
//
// This example layers five toggleable effects on top of the basic
// external-texture sample to showcase what you can do once the YUV frame is
// in WebGPU's hands:
//   * Resize modes (cover / contain / stretch / center 1:1) computed in WGSL
//     from the texture vs canvas aspect ratios.
//   * Color filters (grayscale / sepia / invert / vibrant) as 3x3 matrices.
//   * Color "grade" (warm / cool / wide-gamut-ish boost) layered on top.
//   * Ambient mode: fills the contain-mode bars with a Poisson-disk blur of
//     the same frame, same external texture, sampled many times per pixel.
//   * Liquid-glass control buttons drawn inside WGSL: rounded SDF lenses that
//     refract the underlying video by sampling texture_external at offset uvs.
export const SHADER = /* wgsl */ `
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct Uniforms {
  texSize: vec2f,
  canvasSize: vec2f,
  resizeMode: u32,
  shaderEffect: u32,
  colorSpace: u32,
  ambient: u32,
  liquidGlass: u32,
  time: f32,
  _pad: vec2f,
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

fn computeUvScale(mode: u32) -> vec2f {
  let canvasAR = u.canvasSize.x / u.canvasSize.y;
  let texAR = u.texSize.x / u.texSize.y;
  // 0 = cover, 1 = contain, 2 = stretch, 3 = center (1:1 pixel)
  if (mode == 0u) {
    if (texAR > canvasAR) {
      return vec2f(canvasAR / texAR, 1.0);
    }
    return vec2f(1.0, texAR / canvasAR);
  }
  if (mode == 1u) {
    if (texAR > canvasAR) {
      return vec2f(1.0, texAR / canvasAR);
    }
    return vec2f(canvasAR / texAR, 1.0);
  }
  if (mode == 3u) {
    return vec2f(u.canvasSize.x / u.texSize.x, u.canvasSize.y / u.texSize.y);
  }
  return vec2f(1.0, 1.0);
}

fn sampleTex(uv: vec2f) -> vec3f {
  return textureSampleBaseClampToEdge(srcTex, srcSampler, uv).rgb;
}

fn applyShaderEffect(rgb: vec3f, mode: u32) -> vec3f {
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

fn applyColorSpace(rgb: vec3f, mode: u32) -> vec3f {
  if (mode == 1u) {
    return clamp(rgb * vec3f(1.10, 1.02, 0.86), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 2u) {
    return clamp(rgb * vec3f(0.86, 0.98, 1.16), vec3f(0.0), vec3f(1.0));
  }
  return rgb;
}

fn ambientSample(ndc: vec2f) -> vec3f {
  // The heavy lifting is done by the prepass + separable-gaussian compute
  // chain (see blurShaders.ts). blurredTex is already cover-projected and
  // pre-blurred at 1/4 canvas resolution, so the linear sampler does the
  // final upsample for free. That's what makes the effective kernel huge
  // without each fragment paying for 32+ taps.
  let c = textureSampleLevel(blurredTex, srcSampler, ndc, 0.0).rgb;

  // Lift saturation a touch. YouTube's ambient reads as more vivid than
  // the source so the glow tints the surround rather than greying it.
  let luma = dot(c, vec3f(0.2126, 0.7152, 0.0722));
  return clamp(mix(vec3f(luma), c, 1.2), vec3f(0.0), vec3f(1.0));
}

fn glassSample(ndc: vec2f) -> vec3f {
  let scale = computeUvScale(u.resizeMode);
  let uv = vec2f(0.5) + (ndc - vec2f(0.5)) * scale;
  var c = sampleTex(uv);
  c = applyShaderEffect(c, u.shaderEffect);
  c = applyColorSpace(c, u.colorSpace);
  return c;
}

// 2D SDF utilities for the glass-button icons. Distances are in pixel units
// (the caller passes p in pixel-space relative to the button center).
fn sdTriangle(p: vec2f, a: vec2f, b: vec2f, c: vec2f) -> f32 {
  let e0 = b - a;
  let e1 = c - b;
  let e2 = a - c;
  let v0 = p - a;
  let v1 = p - b;
  let v2 = p - c;
  let pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
  let pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
  let pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
  let s = sign(e0.x * e2.y - e0.y * e2.x);
  let d0 = vec2f(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x));
  let d1 = vec2f(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x));
  let d2 = vec2f(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x));
  let d = min(min(d0, d1), d2);
  return -sqrt(d.x) * sign(d.y);
}

fn sdRoundedBox(p: vec2f, b: vec2f, r: f32) -> f32 {
  let q = abs(p) - b + vec2f(r);
  return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

// kind: 0 = play, 1 = skip-back, 2 = skip-forward.
// s scales the normalized [-1, 1] icon coords into pixel units.
fn iconSdf(p: vec2f, s: f32, kind: u32) -> f32 {
  if (kind == 0u) {
    return sdTriangle(p,
      vec2f(-0.42, -0.55) * s,
      vec2f(-0.42,  0.55) * s,
      vec2f( 0.55,  0.00) * s);
  }
  if (kind == 1u) {
    let bar = sdRoundedBox(
      p - vec2f(-0.55, 0.0) * s,
      vec2f(0.07, 0.55) * s,
      0.04 * s);
    let tri = sdTriangle(p,
      vec2f( 0.50, -0.55) * s,
      vec2f( 0.50,  0.55) * s,
      vec2f(-0.40,  0.00) * s);
    return min(bar, tri);
  }
  let bar = sdRoundedBox(
    p - vec2f(0.55, 0.0) * s,
    vec2f(0.07, 0.55) * s,
    0.04 * s);
  let tri = sdTriangle(p,
    vec2f(-0.50, -0.55) * s,
    vec2f(-0.50,  0.55) * s,
    vec2f( 0.40,  0.00) * s);
  return min(bar, tri);
}

fn liquidGlassButton(pixel: vec2f, center: vec2f, radius: f32, iconKind: u32) -> vec4f {
  let dv = pixel - center;
  let d = length(dv);
  let sd = d - radius;
  if (sd > 1.0) {
    return vec4f(0.0);
  }

  // Glass geometry: a puck with a beveled rim. The dome only curves over
  // the outer "thickness" ring of the disc, leaving the center near-flat
  // for clear see-through. Adapted from the Shadertoy liquid-glass formula.
  let thickness = radius * 0.35;
  let baseHeight = thickness * 5.0;
  let sd_safe = min(sd, 0.0);
  let grad = dv / max(d, 0.0001);

  // Reconstruct a 3D normal as if the surface were a spherical dome over
  // the bevel ring: n_cos goes from 0 (flat top) to 1 (vertical at rim).
  let n_cos = clamp((thickness + sd_safe) / thickness, 0.0, 1.0);
  let n_sin = sqrt(max(1.0 - n_cos * n_cos, 0.0));
  let normal = normalize(vec3f(grad.x * n_cos, grad.y * n_cos, n_sin));

  // Dome height z(sd), used as the start z of the refracted ray.
  let h_x = thickness + sd_safe;
  let h = sqrt(max(thickness * thickness - h_x * h_x, 0.0));

  // Snell's-law refraction with per-channel IOR so the rim shows visible
  // chromatic dispersion. The refracted ray starts at z = h, ends at the
  // virtual background plane at z = -baseHeight, then we sample where it
  // lands in screen-space.
  let incident = vec3f(0.0, 0.0, -1.0);
  let pxToUv = vec2f(1.0) / u.canvasSize;
  let rR = refract(incident, normal, 1.0 / 1.46);
  let rG = refract(incident, normal, 1.0 / 1.50);
  let rB = refract(incident, normal, 1.0 / 1.54);
  let totalDepth = h + baseHeight;
  let lenR = totalDepth / max(-rR.z, 0.001);
  let lenG = totalDepth / max(-rG.z, 0.001);
  let lenB = totalDepth / max(-rB.z, 0.001);
  let uvR = (pixel + rR.xy * lenR) * pxToUv;
  let uvG = (pixel + rG.xy * lenG) * pxToUv;
  let uvB = (pixel + rB.xy * lenB) * pxToUv;
  let refracted = vec3f(
    glassSample(uvR).r,
    glassSample(uvG).g,
    glassSample(uvB).b,
  );

  // Light frost: 4-tap blur centered on the G-channel refraction position.
  // Cheap way to add the milky-glass haze without a real Gaussian pass.
  var frost = vec3f(0.0);
  let f = 5.0 * pxToUv;
  frost = frost + glassSample(uvG + vec2f( f.x,  f.y));
  frost = frost + glassSample(uvG + vec2f(-f.x,  f.y));
  frost = frost + glassSample(uvG + vec2f( f.x, -f.y));
  frost = frost + glassSample(uvG + vec2f(-f.x, -f.y));
  frost = frost * 0.25;
  var col = mix(refracted, frost, 0.18);

  // Blinn-Phong specular from an above-front-left light. Tight exponent
  // gives a sharp highlight typical of glass.
  let lightDir = normalize(vec3f(-0.45, -0.65, 1.0));
  let viewDir = vec3f(0.0, 0.0, 1.0);
  let H = normalize(lightDir + viewDir);
  let NdotH = max(dot(normal, H), 0.0);
  let spec = pow(NdotH, 48.0) * 1.1;
  col = col + vec3f(1.0) * spec;

  // Schlick-Fresnel rim. Brightens grazing angles where (1 - n.z) -> 1.
  let fresnel = pow(max(1.0 - normal.z, 0.0), 3.5);
  col = col + vec3f(0.55) * fresnel;

  // White icon SDF overlay.
  let iconScale = radius * 0.5;
  let iconD = iconSdf(dv, iconScale, iconKind);
  let iconA = 1.0 - smoothstep(-0.7, 0.7, iconD);
  col = mix(col, vec3f(1.0), iconA);

  let aa = 1.0 - smoothstep(-1.0, 0.5, sd);
  return vec4f(col, aa);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4f {
  let ndc = in.uv;
  let scale = computeUvScale(u.resizeMode);
  let uv = vec2f(0.5) + (ndc - vec2f(0.5)) * scale;
  let inside = uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;

  var color: vec3f;
  if (inside) {
    color = sampleTex(uv);
  } else {
    color = vec3f(0.0);
  }

  if (u.ambient != 0u && !inside) {
    color = ambientSample(ndc);
    // "On" mode reads as a glow rather than a backdrop: blend toward black
    // so the surround stays subdued next to the video.
    if (u.ambient == 2u) {
      color = color * 0.75;
    }
  }

  color = applyShaderEffect(color, u.shaderEffect);
  color = applyColorSpace(color, u.colorSpace);

  if (u.liquidGlass == 1u) {
    // Centered on screen for landscape playback. Center button is the play
    // control and noticeably larger than the two skip controls.
    let pixel = ndc * u.canvasSize;
    let cx = u.canvasSize.x * 0.5;
    let cy = u.canvasSize.y * 0.5;
    let R = min(u.canvasSize.x, u.canvasSize.y) * 0.13;  // center (play)
    let r = R * 0.65;                                    // skip back/forward
    let sp = R + r + R * 0.45;                           // center-to-center spacing
    let b1 = liquidGlassButton(pixel, vec2f(cx - sp, cy), r, 1u);
    let b2 = liquidGlassButton(pixel, vec2f(cx,      cy), R, 0u);
    let b3 = liquidGlassButton(pixel, vec2f(cx + sp, cy), r, 2u);
    color = mix(color, b1.rgb, b1.a);
    color = mix(color, b2.rgb, b2.a);
    color = mix(color, b3.rgb, b3.a);
  }

  return vec4f(color, 1.0);
}
`;
