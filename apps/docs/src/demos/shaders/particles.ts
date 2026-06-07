export const PARTICLE_COUNT = 2048;

export const particlesComputeWGSL = /* wgsl */ `
struct Particle {
  pos: vec2f,
  vel: vec2f,
}

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: vec2f;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3u) {
  let i = gid.x;
  if (i >= arrayLength(&particles)) {
    return;
  }

  var p = particles[i];
  p.pos += p.vel * params.x;
  p.vel.y -= params.y;

  if (p.pos.x < -1.0 || p.pos.x > 1.0) {
    p.vel.x *= -1.0;
  }
  if (p.pos.y < -1.0) {
    p.pos = vec2f(fract(sin(f32(i) * 12.9898) * 43758.5453) * 2.0 - 1.0, 1.0);
    p.vel = vec2f((fract(cos(f32(i)) * 123.456) - 0.5) * 0.01, 0.008 + fract(sin(f32(i))) * 0.006);
  }

  particles[i] = p;
}
`;

export const particlesRenderWGSL = /* wgsl */ `
struct Particle {
  pos: vec2f,
  vel: vec2f,
}

@group(0) @binding(0) var<storage, read> particles: array<Particle>;

struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) color: vec4f,
}

@vertex
fn vs_main(
  @builtin(instance_index) i: u32,
  @builtin(vertex_index) vi: u32,
) -> VsOut {
  let quad = array(
    vec2f(-0.012, -0.012),
    vec2f( 0.012, -0.012),
    vec2f(-0.012,  0.012),
    vec2f(-0.012,  0.012),
    vec2f( 0.012, -0.012),
    vec2f( 0.012,  0.012),
  );
  let p = particles[i];
  let hue = fract(f32(i) * 0.013 + length(p.vel) * 40.0);
  var out: VsOut;
  out.position = vec4f(p.pos + quad[vi], 0.0, 1.0);
  out.color = vec4f(
    0.5 + 0.5 * sin(hue * 6.28),
    0.5 + 0.5 * sin(hue * 6.28 + 2.0),
    0.5 + 0.5 * sin(hue * 6.28 + 4.0),
    0.65,
  );
  return out;
}

@fragment
fn fs_main(@location(0) color: vec4f) -> @location(0) vec4f {
  return color;
}
`;
