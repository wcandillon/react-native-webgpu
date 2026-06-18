#include <metal_stdlib>
#include <SwiftUI/SwiftUI_Metal.h>

using namespace metal;

// Path B iOS shader: the Metal counterpart of the Android AGSL shader.
// Used as a SwiftUI .layerEffect. `position` is in user-space points and
// `layer.sample` samples the rasterized view content at a point. `size` is the
// view size in points, passed from Swift. Vignette + edge chromatic aberration.
[[ stitchable ]] half4 vignetteAberration(float2 position,
                                          SwiftUI::Layer layer,
                                          float2 size) {
  float2 uv = position / size;
  float2 centered = uv - 0.5;
  float dist = length(centered);

  // Constant horizontal RGB split (visible everywhere, even at center) that
  // grows toward the edges. Deliberately bold so the effect is obvious.
  float split = (0.012 + dist * 0.04) * size.x;

  half4 c;
  c.r = layer.sample(position + float2(split, 0.0)).r;
  c.g = layer.sample(position).g;
  c.b = layer.sample(position - float2(split, 0.0)).b;
  c.a = 1.0;

  // Strong vignette: full brightness in the center, hard falloff at the edges.
  // (smoothstep requires edge0 < edge1, so invert rather than swapping edges.)
  float vignette = 1.0 - smoothstep(0.25, 0.95, dist);
  vignette *= vignette;
  c.rgb *= half(vignette);
  return c;
}
