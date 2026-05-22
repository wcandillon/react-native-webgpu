const EFFECT_LABELS = ["Off", "Gray", "Sepia", "Invert", "Vibrant"] as const;
const TINT_LABELS = ["Off", "Warm", "Cool"] as const;
const ABERRATION_LABELS = ["Off", "Soft", "Strong"] as const;
const TOGGLE_LABELS = ["Off", "On"] as const;

export type Modes = {
  effect: number;
  tint: number;
  aberration: number;
  vignette: number;
  pixelate: number;
};

export const INITIAL_MODES: Modes = {
  effect: 0,
  tint: 0,
  aberration: 1,
  vignette: 0,
  pixelate: 0,
};

// Aberration strength in UV units per level. Soft matches the original demo.
export const ABERRATION_STRENGTHS = [0.0, 0.006, 0.018] as const;
// Block size in UV units per pixelate level. Larger value, chunkier pixels.
export const PIXELATE_BLOCKS = [0.0, 0.02] as const;

export type Feature = {
  title: string;
  key: keyof Modes;
  labels: readonly string[];
};

export const FEATURES: Feature[] = [
  { title: "Effect", key: "effect", labels: EFFECT_LABELS },
  { title: "Tint", key: "tint", labels: TINT_LABELS },
  { title: "Aberration", key: "aberration", labels: ABERRATION_LABELS },
  { title: "Vignette", key: "vignette", labels: TOGGLE_LABELS },
  { title: "Pixelate", key: "pixelate", labels: TOGGLE_LABELS },
];
