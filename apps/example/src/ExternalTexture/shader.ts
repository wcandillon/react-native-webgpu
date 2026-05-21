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
  if (mode == 3u) {
    // Rec.709 -> Display P3 (approximate, clamped). Demonstrates a
    // primaries-conversion matrix on the GPU side after the YUV pipeline.
    let m = mat3x3f(
      vec3f( 1.2249, -0.2247,  0.0000),
      vec3f(-0.0420,  1.0419,  0.0000),
      vec3f(-0.0197, -0.0786,  1.0979),
    );
    return clamp(m * rgb, vec3f(0.0), vec3f(1.0));
  }
  // Tone-mapping operators 4..7. None of these "fix" the 10->8 NV12
  // downconvert colorimetrically; they're perceptual rescues that reshape the
  // S-curve and re-saturate the result. Useful for A/B comparison on the
  // Andor-style HDR-decoded-flat source.
  if (mode == 4u) {
    // ACES filmic (Krzysztof Narkowicz fit). Punchy contrast, warm shadows.
    let toned = (rgb * (2.51 * rgb + 0.03)) /
                (rgb * (2.43 * rgb + 0.59) + 0.14);
    let c = clamp(toned, vec3f(0.0), vec3f(1.0));
    let l = dot(c, vec3f(0.2126, 0.7152, 0.0722));
    return clamp(mix(vec3f(l), c, 1.4), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 5u) {
    // Reinhard x/(1+x): soft, no clipping. Always slightly desaturating,
    // so a sat lift comes after.
    let toned = rgb / (vec3f(1.0) + rgb);
    let l = dot(toned, vec3f(0.2126, 0.7152, 0.0722));
    return clamp(mix(vec3f(l), toned, 1.35), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 6u) {
    // Hable "Uncharted 2" filmic. Strong toe + shoulder, very contrasty.
    let A = 0.15; let B = 0.50; let C = 0.10;
    let D = 0.20; let E = 0.02; let F = 0.30;
    let exposed = rgb * 2.0;
    let curr = ((exposed * (A * exposed + C * B) + D * E) /
                (exposed * (A * exposed + B) + D * F)) - E / F;
    let W = vec3f(11.2);
    let whiteScale = vec3f(1.0) /
                     (((W * (A * W + C * B) + D * E) /
                       (W * (A * W + B) + D * F)) - E / F);
    let toned = clamp(curr * whiteScale, vec3f(0.0), vec3f(1.0));
    let l = dot(toned, vec3f(0.2126, 0.7152, 0.0722));
    return clamp(mix(vec3f(l), toned, 1.3), vec3f(0.0), vec3f(1.0));
  }
  if (mode == 8u) {
    // HDR decode for BT.2020 + SMPTE ST 2084 (PQ) sources like the Andor
    // clip (code points 9-16-9). importExternalTexture used the BT.2020
    // YCbCr matrix when converting YUV->RGB but the resulting RGB values
    // still carry the PQ non-linear encoding. The chain:
    //   1. PQ EOTF -> linear scene light, 1.0 = 10000 nits
    //   2. scale so SDR reference white (100 nits) sits at 1.0
    //   3. BT.2020 -> BT.709 primaries
    //   4. ACES tone-map to fit HDR range into [0,1]
    //   5. sRGB OETF so the bgra8unorm canvas displays the right values.
    let m1 = 0.1593017578125;
    let m2 = 78.84375;
    let c1 = 0.8359375;
    let c2 = 18.8515625;
    let c3 = 18.6875;
    let cp = pow(max(rgb, vec3f(0.0)), vec3f(1.0 / m2));
    let num = max(cp - vec3f(c1), vec3f(0.0));
    let den = vec3f(c2) - vec3f(c3) * cp;
    let linearPq = pow(num / den, vec3f(1.0 / m1));
    let scene = linearPq * 100.0;

    // BT.2020 -> BT.709 (column-major; each vec3f is one column).
    let m709 = mat3x3f(
      vec3f( 1.6605, -0.1246, -0.0182),
      vec3f(-0.5876,  1.1329, -0.1006),
      vec3f(-0.0728, -0.0083,  1.1187),
    );
    let in709 = max(m709 * scene, vec3f(0.0));

    let toned = (in709 * (2.51 * in709 + 0.03)) /
                (in709 * (2.43 * in709 + 0.59) + 0.14);
    let sdr = clamp(toned, vec3f(0.0), vec3f(1.0));

    let cutoff = vec3f(0.0031308);
    let high = vec3f(1.055) * pow(sdr, vec3f(1.0 / 2.4)) - vec3f(0.055);
    let low = vec3f(12.92) * sdr;
    return select(low, high, sdr > cutoff);
  }
  if (mode == 7u) {
    // AgX (Troy Sobotka), three.js's polynomial-sigmoid fit. Modern look,
    // preserves saturation better than ACES on bright primaries.
    let agx_mat = mat3x3f(
      vec3f(0.842479062253094, 0.0423282422610123, 0.0423756549057051),
      vec3f(0.0784335999999992, 0.878468636469772, 0.0784336),
      vec3f(0.0792237451477643, 0.0791661274605434, 0.879142973793104),
    );
    var c = agx_mat * rgb;
    c = clamp(log2(max(c, vec3f(1e-6))), vec3f(-10.0), vec3f(6.5));
    c = (c + vec3f(10.0)) / 16.5;
    c = vec3f(0.5) + vec3f(0.5) *
        sin(((vec3f(-3.11) * c + vec3f(6.42)) * c - vec3f(0.378)) * c - vec3f(1.44));
    let agx_inv = mat3x3f(
      vec3f( 1.19687900512017, -0.0528968517574562, -0.0529716355144438),
      vec3f(-0.0980208811401368, 1.15190312990417,  -0.0980434501171241),
      vec3f(-0.0990297440797205, -0.0989611768448433, 1.15107367264116),
    );
    return clamp(agx_inv * c, vec3f(0.0), vec3f(1.0));
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

fn liquidGlassButton(pixel: vec2f, center: vec2f, radius: f32, tint: vec3f) -> vec4f {
  let dv = pixel - center;
  let d = length(dv);
  if (d > radius + 1.5) {
    return vec4f(0.0);
  }
  let edgeT = clamp(1.0 - d / radius, 0.0, 1.0);
  let nrm = dv / max(d, 0.0001);
  // Bend more strongly near the rim, then taper to zero exactly at the rim.
  let refractStrength = pow(1.0 - edgeT, 1.8) * (1.0 - smoothstep(0.92, 1.0, d / radius)) * 22.0;
  let offset = -nrm * refractStrength / u.canvasSize;
  let baseNdc = pixel / u.canvasSize;
  let refracted = glassSample(baseNdc + offset);
  // Frosted glass: also pick up a slightly displaced 4-tap blur and mix.
  var frost = vec3f(0.0);
  let f = 4.0 / u.canvasSize;
  frost = frost + glassSample(baseNdc + vec2f( f.x,  f.y));
  frost = frost + glassSample(baseNdc + vec2f(-f.x,  f.y));
  frost = frost + glassSample(baseNdc + vec2f( f.x, -f.y));
  frost = frost + glassSample(baseNdc + vec2f(-f.x, -f.y));
  frost = frost * 0.25;
  var col = mix(refracted, frost, 0.35);
  // Tint and top-down specular.
  let topHi = smoothstep(0.55, 1.0, edgeT) * smoothstep(0.4, -0.6, nrm.y);
  let rim = smoothstep(0.96, 1.0, d / radius) * (1.0 - smoothstep(1.0, 1.05, d / radius));
  col = mix(col, tint, 0.12);
  col = col + vec3f(0.55) * topHi;
  col = col + vec3f(0.9) * rim * 0.6;
  let aa = 1.0 - smoothstep(radius - 1.0, radius + 0.5, d);
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
    let pixel = ndc * u.canvasSize;
    let cx = u.canvasSize.x * 0.5;
    let cy = u.canvasSize.y * 0.78;
    let r = min(u.canvasSize.x, u.canvasSize.y) * 0.075;
    let sp = r * 2.7;
    let b1 = liquidGlassButton(pixel, vec2f(cx - sp, cy), r, vec3f(1.00, 0.55, 0.30));
    let b2 = liquidGlassButton(pixel, vec2f(cx,      cy), r * 1.15, vec3f(0.45, 0.85, 1.00));
    let b3 = liquidGlassButton(pixel, vec2f(cx + sp, cy), r, vec3f(1.00, 0.42, 0.65));
    color = mix(color, b1.rgb, b1.a);
    color = mix(color, b2.rgb, b2.a);
    color = mix(color, b3.rgb, b3.a);
  }

  return vec4f(color, 1.0);
}
`;
