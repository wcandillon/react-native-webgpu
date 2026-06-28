// The hero shader gallery. One entry is picked at random on each page load (no
// auto-rotation; reload to get a different one).
//
// Adding a first-party shader: drop a file in ./shaders exporting a ShaderEntry
// and add it below.
//
// Adding a compute.toys shader: add its id and run `yarn fetch:shaders <id>`
// (see scripts/fetch-computetoys.mjs). That generates ./shaders/generated/ct-<id>.ts
// with the source + real uniform defaults + attribution. Import it here and
// apply any per-hero tuning (orbitPeriod, maxDimension). Credit is not a
// license: compute.toys exposes none, so these are used with attribution.

import { ct202 } from "./shaders/generated";
import type { ComputeToysShader, ShaderEntry } from "./types";

// "Black Hole Particles Bokeh" by michael0884, tuned for the hero (slow orbit,
// capped internal resolution to keep the per-pixel particle loop off the GPU
// timeout path). A dark shader, so the overlay uses light text/buttons.
const blackHole: ShaderEntry = {
  ...ct202,
  appearance: "dark",
  shader: {
    ...(ct202.shader as ComputeToysShader),
    orbitPeriod: 90,
    maxDimension: 900,
  },
};

export const SHADERS: ShaderEntry[] = [blackHole];
