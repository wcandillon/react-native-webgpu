/// <reference types="jest" />

import { createBlurPassPlan } from "../blurPlan";

describe("createBlurPassPlan", () => {
  it("alternates horizontal and vertical passes for every iteration", () => {
    expect(
      createBlurPassPlan({
        width: 512,
        height: 256,
        filterSize: 15,
        iterations: 2,
      }),
    ).toEqual({
      blockDim: 114,
      filterDim: 15,
      passes: [
        { bindGroup: 0, x: 5, y: 64 },
        { bindGroup: 1, x: 3, y: 128 },
        { bindGroup: 2, x: 5, y: 64 },
        { bindGroup: 1, x: 3, y: 128 },
      ],
    });
  });

  it("rejects unsupported filter sizes and iteration counts", () => {
    expect(() =>
      createBlurPassPlan({
        width: 512,
        height: 256,
        filterSize: 34,
        iterations: 1,
      }),
    ).toThrow("filterSize must be an odd integer between 1 and 33");

    expect(() =>
      createBlurPassPlan({
        width: 512,
        height: 256,
        filterSize: 15,
        iterations: 0,
      }),
    ).toThrow("iterations must be a positive integer");
  });
});
