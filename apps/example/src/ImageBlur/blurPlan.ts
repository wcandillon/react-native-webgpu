const TILE_DIMENSION = 128;
const BATCH_HEIGHT = 4;

export type BlurBindGroupIndex = 0 | 1 | 2;

export interface BlurPass {
  bindGroup: BlurBindGroupIndex;
  x: number;
  y: number;
}

interface BlurPlanOptions {
  width: number;
  height: number;
  filterSize: number;
  iterations: number;
}

export const createBlurPassPlan = ({
  width,
  height,
  filterSize,
  iterations,
}: BlurPlanOptions) => {
  if (
    !Number.isInteger(filterSize) ||
    filterSize < 1 ||
    filterSize > 33 ||
    filterSize % 2 === 0
  ) {
    throw new Error("filterSize must be an odd integer between 1 and 33");
  }
  if (!Number.isInteger(iterations) || iterations < 1) {
    throw new Error("iterations must be a positive integer");
  }

  const blockDim = TILE_DIMENSION - (filterSize - 1);
  const horizontalPass = {
    x: Math.ceil(width / blockDim),
    y: Math.ceil(height / BATCH_HEIGHT),
  };
  const verticalPass = {
    x: Math.ceil(height / blockDim),
    y: Math.ceil(width / BATCH_HEIGHT),
  };
  const passes: BlurPass[] = [
    { bindGroup: 0, ...horizontalPass },
    { bindGroup: 1, ...verticalPass },
  ];

  for (let index = 1; index < iterations; index += 1) {
    passes.push(
      { bindGroup: 2, ...horizontalPass },
      { bindGroup: 1, ...verticalPass },
    );
  }

  return { blockDim, filterDim: filterSize, passes };
};
