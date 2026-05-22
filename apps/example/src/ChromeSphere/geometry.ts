// Sphere mesh generator for the chrome scene. Returns interleaved
// position+normal vertices and a triangle index list. The shader uses the
// upper-left 3x3 of the model matrix to transform normals, so the sphere is
// built with unit-length normals (no non-uniform scale at generation time).

export type Mesh = {
  vertices: Float32Array; // [px, py, pz, nx, ny, nz] * N
  indices: Uint16Array;
};

const FLOATS_PER_VERTEX = 6;

export const VERTEX_STRIDE_BYTES = FLOATS_PER_VERTEX * 4;
export const POSITION_OFFSET = 0;
export const NORMAL_OFFSET = 12;

export function generateSphere(
  radius: number,
  latBands: number,
  lonBands: number,
): Mesh {
  const verts: number[] = [];
  const idx: number[] = [];
  for (let lat = 0; lat <= latBands; lat++) {
    const theta = (lat * Math.PI) / latBands;
    const sinTheta = Math.sin(theta);
    const cosTheta = Math.cos(theta);
    for (let lon = 0; lon <= lonBands; lon++) {
      const phi = (lon * 2 * Math.PI) / lonBands;
      const sinPhi = Math.sin(phi);
      const cosPhi = Math.cos(phi);
      const nx = cosPhi * sinTheta;
      const ny = cosTheta;
      const nz = sinPhi * sinTheta;
      verts.push(nx * radius, ny * radius, nz * radius, nx, ny, nz);
    }
  }
  const stride = lonBands + 1;
  for (let lat = 0; lat < latBands; lat++) {
    for (let lon = 0; lon < lonBands; lon++) {
      const a = lat * stride + lon;
      const b = a + stride;
      idx.push(a, b, a + 1, b, b + 1, a + 1);
    }
  }
  return {
    vertices: new Float32Array(verts),
    indices: new Uint16Array(idx),
  };
}
