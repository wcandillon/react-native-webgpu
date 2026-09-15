import * as THREE from "three";

// Procedural face props. Each builder returns a group modelled in a local
// frame where 1 unit is the distance between the eye centers, +y points to
// the top of the head, +z toward the camera, and the origin sits on the
// prop's anchor landmark. FaceMesh.tsx positions, orients and scales the
// group from the live landmarks.

const dark = (color: number, metalness = 0.2, roughness = 0.6) =>
  new THREE.MeshStandardMaterial({
    color,
    metalness,
    roughness,
    side: THREE.DoubleSide,
  });

// Round sunglasses centered between the eyes.
export const makeGlasses = (): THREE.Group => {
  const group = new THREE.Group();
  const frame = dark(0x151515, 0.7, 0.3);
  const lens = new THREE.MeshBasicMaterial({
    color: 0x1a2a3a,
    transparent: true,
    opacity: 0.55,
    side: THREE.DoubleSide,
  });
  for (const x of [-0.5, 0.5]) {
    const ring = new THREE.Mesh(
      new THREE.TorusGeometry(0.33, 0.04, 12, 40),
      frame,
    );
    ring.position.set(x, 0, 0);
    group.add(ring);
    const glass = new THREE.Mesh(new THREE.CircleGeometry(0.31, 40), lens);
    glass.position.set(x, 0, -0.005);
    group.add(glass);
    // Temple running back along the side of the head.
    const temple = new THREE.Mesh(
      new THREE.BoxGeometry(0.04, 0.04, 1.4),
      frame,
    );
    temple.position.set(Math.sign(x) * 0.86, 0.05, -0.65);
    group.add(temple);
  }
  const bridge = new THREE.Mesh(new THREE.BoxGeometry(0.34, 0.04, 0.04), frame);
  bridge.position.set(0, 0.06, 0);
  group.add(bridge);
  return group;
};

// Top hat whose brim sits at the origin and whose crown rises along +y.
export const makeTopHat = (): THREE.Group => {
  const group = new THREE.Group();
  const felt = dark(0x101010, 0.05, 0.85);
  const crown = new THREE.Mesh(
    new THREE.CylinderGeometry(0.78, 0.84, 1.15, 40),
    felt,
  );
  crown.position.set(0, 0.6, 0);
  group.add(crown);
  const brim = new THREE.Mesh(
    new THREE.CylinderGeometry(1.4, 1.4, 0.06, 48),
    felt,
  );
  brim.position.set(0, 0.03, 0);
  group.add(brim);
  const band = new THREE.Mesh(
    new THREE.CylinderGeometry(0.855, 0.855, 0.2, 40),
    dark(0xb0122b, 0.1, 0.7),
  );
  band.position.set(0, 0.18, 0);
  group.add(band);
  return group;
};

// Handlebar mustache drawn as a 2D shape and extruded a little.
export const makeMustache = (): THREE.Group => {
  const group = new THREE.Group();
  const shape = new THREE.Shape();
  shape.moveTo(0, 0.06);
  shape.bezierCurveTo(0.3, 0.26, 0.68, 0.22, 0.86, 0.06);
  shape.bezierCurveTo(1.0, -0.06, 0.8, -0.24, 0.62, -0.14);
  shape.bezierCurveTo(0.42, -0.04, 0.2, -0.16, 0, -0.2);
  shape.bezierCurveTo(-0.2, -0.16, -0.42, -0.04, -0.62, -0.14);
  shape.bezierCurveTo(-0.8, -0.24, -1.0, -0.06, -0.86, 0.06);
  shape.bezierCurveTo(-0.68, 0.22, -0.3, 0.26, 0, 0.06);
  const geometry = new THREE.ExtrudeGeometry(shape, {
    depth: 0.08,
    bevelEnabled: true,
    bevelThickness: 0.03,
    bevelSize: 0.02,
    bevelSegments: 2,
    curveSegments: 24,
  });
  const mesh = new THREE.Mesh(geometry, dark(0x2b1a0e, 0.0, 0.9));
  mesh.position.set(0, 0, -0.04);
  group.add(mesh);
  return group;
};
