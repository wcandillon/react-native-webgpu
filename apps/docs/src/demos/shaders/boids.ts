export const BOID_COUNT = 256;

export const boidsComputeWGSL = /* wgsl */ `
struct Boid {
  pos: vec2f,
  vel: vec2f,
}

@group(0) @binding(0) var<storage, read> input: array<Boid>;
@group(0) @binding(1) var<storage, read_write> output: array<Boid>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
  let i = gid.x;
  if (i >= arrayLength(&input)) {
    return;
  }

  var b = input[i];
  var separation = vec2f(0.0);
  var alignment = vec2f(0.0);
  var alignCount = 0u;

  for (var j = 0u; j < arrayLength(&input); j++) {
    if (j == i) {
      continue;
    }
    let other = input[j];
    let delta = b.pos - other.pos;
    let dist = length(delta);
    if (dist < 0.12 && dist > 0.0001) {
      separation += delta / dist;
    }
    if (dist < 0.35) {
      alignment += other.vel;
      alignCount++;
    }
  }

  if (alignCount > 0u) {
    alignment /= f32(alignCount);
  }

  b.vel += separation * 0.0008 + alignment * 0.02;
  let speed = length(b.vel);
  b.vel = select(normalize(b.vel) * 0.004, b.vel, speed < 0.0001);
  b.pos += b.vel;

  if (b.pos.x > 1.0) { b.pos.x = -1.0; }
  if (b.pos.x < -1.0) { b.pos.x = 1.0; }
  if (b.pos.y > 1.0) { b.pos.y = -1.0; }
  if (b.pos.y < -1.0) { b.pos.y = 1.0; }

  output[i] = b;
}
`;

export const boidsRenderWGSL = /* wgsl */ `
struct Boid {
  pos: vec2f,
  vel: vec2f,
}

@group(0) @binding(0) var<storage, read> boids: array<Boid>;

struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) color: vec4f,
}

@vertex
fn vs_main(
  @builtin(instance_index) i: u32,
  @builtin(vertex_index) vi: u32,
) -> VsOut {
  let tri = array(
    vec2f(0.0, 0.035),
    vec2f(-0.02, -0.02),
    vec2f(0.02, -0.02),
  );
  let b = boids[i];
  let angle = atan2(b.vel.y, b.vel.x);
  let c = cos(angle);
  let s = sin(angle);
  let local = tri[vi];
  let rotated = vec2f(local.x * c - local.y * s, local.x * s + local.y * c);
  var out: VsOut;
  out.position = vec4f(b.pos + rotated, 0.0, 1.0);
  let n = normalize(b.vel + vec2f(0.001, 0.001));
  out.color = vec4f(0.5 + 0.5 * n.x, 0.5 + 0.5 * n.y, 0.5, 1.0);
  return out;
}

@fragment
fn fs_main(@location(0) color: vec4f) -> @location(0) vec4f {
  return color;
}
`;
