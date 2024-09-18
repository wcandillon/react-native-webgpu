export const vertWGSL = `
struct Output {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
}

@vertex
fn main(
  @builtin(vertex_index) vertexIndex: u32,
) -> Output {
  var pos = array<vec2f, 4>(
    vec2(1, 1), // top-right
    vec2(-1, 1), // top-left
    vec2(1, -1), // bottom-right
    vec2(-1, -1) // bottom-left
  );

  var uv = array<vec2f, 4>(
    vec2(1., 1.), // top-right
    vec2(0., 1.), // top-left
    vec2(1., 0.), // bottom-right
    vec2(0., 0.) // bottom-left
  );

  var out: Output;
  out.pos = vec4f(pos[vertexIndex], 0.0, 1.0);
  out.uv = uv[vertexIndex];
  return out;
}`;

export const fragWGSL = `
struct Span {
  x: u32,
  y: u32,
}

@group(0) @binding(0) var<uniform> span: Span;

@fragment
fn main(
  @location(0) uv: vec2f,
) -> @location(0) vec4f {
  let red = floor(uv.x * f32(span.x)) / f32(span.x);
  let green = floor(uv.y * f32(span.y)) / f32(span.y);
  return vec4(red, green, 0.5, 1.0);
}`;
