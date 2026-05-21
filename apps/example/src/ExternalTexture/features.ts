const RESIZE_LABELS = ["Cover", "Contain", "Stretch", "Center"] as const;
const EFFECT_LABELS = ["Off", "Gray", "Sepia", "Invert", "Vibrant"] as const;
const COLOR_LABELS = [
  "Normal",
  "Warm",
  "Cool",
  "P3",
  "ACES",
  "Reinhard",
  "Filmic",
  "AgX",
  "HDR",
] as const;
const AMBIENT_LABELS = ["Off", "Blur", "On"] as const;
const TOGGLE_LABELS = ["Off", "On"] as const;

export type Modes = {
  resize: number;
  effect: number;
  color: number;
  ambient: number;
  glass: number;
};

export const INITIAL_MODES: Modes = {
  resize: 0,
  effect: 0,
  color: 0,
  ambient: 0,
  glass: 0,
};

export type Feature = {
  title: string;
  key: keyof Modes;
  labels: readonly string[];
};

export const FEATURES: Feature[] = [
  { title: "Fit", key: "resize", labels: RESIZE_LABELS },
  { title: "Effect", key: "effect", labels: EFFECT_LABELS },
  { title: "Color", key: "color", labels: COLOR_LABELS },
  { title: "Ambient", key: "ambient", labels: AMBIENT_LABELS },
  { title: "Glass", key: "glass", labels: TOGGLE_LABELS },
];
