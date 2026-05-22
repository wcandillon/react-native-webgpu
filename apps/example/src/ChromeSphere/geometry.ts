// Tiny mesh generators for the chrome scene. Each returns interleaved
// position+normal vertices and a triangle index list. The shader uses the
// upper-left 3x3 of the model matrix to transform normals, so all shapes
// here must be built with unit-length normals (no non-uniform scale at
// generation time).

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

export function generateCube(size: number): Mesh {
  const s = size / 2;
  // Each face has its own 4 vertices so face normals stay flat (no smoothing
  // across edges, which would defeat the cube-as-mirror look).
  const faces: {
    n: [number, number, number];
    corners: [number, number, number][];
  }[] = [
    {
      n: [1, 0, 0],
      corners: [
        [s, -s, -s],
        [s, s, -s],
        [s, s, s],
        [s, -s, s],
      ],
    },
    {
      n: [-1, 0, 0],
      corners: [
        [-s, -s, s],
        [-s, s, s],
        [-s, s, -s],
        [-s, -s, -s],
      ],
    },
    {
      n: [0, 1, 0],
      corners: [
        [-s, s, -s],
        [-s, s, s],
        [s, s, s],
        [s, s, -s],
      ],
    },
    {
      n: [0, -1, 0],
      corners: [
        [-s, -s, s],
        [-s, -s, -s],
        [s, -s, -s],
        [s, -s, s],
      ],
    },
    {
      n: [0, 0, 1],
      corners: [
        [-s, -s, s],
        [s, -s, s],
        [s, s, s],
        [-s, s, s],
      ],
    },
    {
      n: [0, 0, -1],
      corners: [
        [s, -s, -s],
        [-s, -s, -s],
        [-s, s, -s],
        [s, s, -s],
      ],
    },
  ];
  const verts: number[] = [];
  const idx: number[] = [];
  faces.forEach((face, fi) => {
    face.corners.forEach((c) => {
      verts.push(c[0], c[1], c[2], face.n[0], face.n[1], face.n[2]);
    });
    const base = fi * 4;
    idx.push(base, base + 1, base + 2, base, base + 2, base + 3);
  });
  return {
    vertices: new Float32Array(verts),
    indices: new Uint16Array(idx),
  };
}

export function generateTorus(
  majorRadius: number,
  minorRadius: number,
  majorSegments: number,
  minorSegments: number,
): Mesh {
  const verts: number[] = [];
  const idx: number[] = [];
  for (let i = 0; i <= majorSegments; i++) {
    const u = (i / majorSegments) * 2 * Math.PI;
    const cu = Math.cos(u);
    const su = Math.sin(u);
    for (let j = 0; j <= minorSegments; j++) {
      const v = (j / minorSegments) * 2 * Math.PI;
      const cv = Math.cos(v);
      const sv = Math.sin(v);
      const x = (majorRadius + minorRadius * cv) * cu;
      const y = minorRadius * sv;
      const z = (majorRadius + minorRadius * cv) * su;
      // Outward-from-minor-circle normal, unit length by construction.
      const nx = cv * cu;
      const ny = sv;
      const nz = cv * su;
      verts.push(x, y, z, nx, ny, nz);
    }
  }
  const stride = minorSegments + 1;
  for (let i = 0; i < majorSegments; i++) {
    for (let j = 0; j < minorSegments; j++) {
      const a = i * stride + j;
      const b = a + stride;
      idx.push(a, b, a + 1, b, b + 1, a + 1);
    }
  }
  return {
    vertices: new Float32Array(verts),
    indices: new Uint16Array(idx),
  };
}
