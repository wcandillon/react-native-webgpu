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

// "Black Hole Particles Bokeh" by michael0884, tuned for the hero: drag to
// rotate the camera (compute.toys gesture), static otherwise. Internal
// resolution is capped to keep the per-pixel particle loop off the GPU timeout
// path. A dark shader, so the overlay uses light text/buttons.
const blackHole: ShaderEntry = {
  ...ct202,
  appearance: "dark",
  shader: {
    ...(ct202.shader as ComputeToysShader),
    maxDimension: 900,
    // Phones get a lower internal resolution and a 64x64 particle grid (4x fewer
    // particles than the 128x128 desktop grid) to keep the per-pixel bokeh loop
    // off the GPU timeout path. PARTICLE_COUNT_16 must stay PARTICLE_COUNT / 16.
    mobileMaxDimension: 600,
    mobileConstOverrides: { PARTICLE_COUNT: 64, PARTICLE_COUNT_16: 4 },
    initialView: [0.5, 0.75],
  },
};

export const SHADERS: ShaderEntry[] = [blackHole];
