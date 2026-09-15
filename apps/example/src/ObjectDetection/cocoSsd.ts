import * as tf from "@tensorflow/tfjs";
import { CLASSES } from "@tensorflow-models/coco-ssd/dist/classes";

// Async port of @tensorflow-models/coco-ssd's detect(). The package reads
// the graph's outputs with dataSync(), which on the tfjs WebGPU backend goes
// through an OffscreenCanvas 2D context that React Native does not have.
// Same graph, same post-processing, but every GPU read awaits data().
const MODEL_URL =
  "https://storage.googleapis.com/tfjs-models/savedmodel/ssdlite_mobilenet_v2/model.json";
const IOU_THRESHOLD = 0.5;

export interface DetectedObject {
  // (x, y, width, height) in input pixels.
  bbox: [number, number, number, number];
  class: string;
  score: number;
}

export interface CocoSsd {
  detect(
    image: tf.Tensor3D,
    maxNumBoxes?: number,
    minScore?: number,
  ): Promise<DetectedObject[]>;
  dispose(): void;
}

export const loadCocoSsd = async (): Promise<CocoSsd> => {
  const model = await tf.loadGraphModel(MODEL_URL);
  // Warm up so the first real frame does not pay for shader compilation.
  const zeros = tf.zeros([1, 300, 300, 3], "int32");
  const warmup = (await model.executeAsync(zeros)) as tf.Tensor[];
  tf.dispose([zeros, ...warmup]);

  const detect = async (
    image: tf.Tensor3D,
    maxNumBoxes = 20,
    minScore = 0.5,
  ): Promise<DetectedObject[]> => {
    const [height, width] = image.shape;
    const batched = tf.expandDims(image, 0);
    let result: tf.Tensor[];
    try {
      result = (await model.executeAsync(batched)) as tf.Tensor[];
    } finally {
      batched.dispose();
    }
    const [scoresTensor, boxesTensor] = result;
    const numBoxes = scoresTensor.shape[1] as number;
    const numClasses = scoresTensor.shape[2] as number;
    const boxesShape: [number, number] = [
      boxesTensor.shape[1] as number,
      boxesTensor.shape[3] as number,
    ];
    let scores: tf.TypedArray;
    let boxes: tf.TypedArray;
    try {
      [scores, boxes] = await Promise.all([
        scoresTensor.data(),
        boxesTensor.data(),
      ]);
    } finally {
      tf.dispose(result);
    }

    // Best class per anchor box.
    const maxScores = new Float32Array(numBoxes);
    const classes = new Int32Array(numBoxes);
    for (let i = 0; i < numBoxes; i++) {
      let best = -1;
      let bestIndex = 0;
      for (let j = 0; j < numClasses; j++) {
        const s = scores[i * numClasses + j];
        if (s > best) {
          best = s;
          bestIndex = j;
        }
      }
      maxScores[i] = best;
      classes[i] = bestIndex;
    }

    const boxes2d = tf.tensor2d(boxes, boxesShape);
    let indexes: tf.TypedArray;
    try {
      const indexTensor = await tf.image.nonMaxSuppressionAsync(
        boxes2d,
        maxScores,
        maxNumBoxes,
        IOU_THRESHOLD,
        minScore,
      );
      try {
        indexes = await indexTensor.data();
      } finally {
        indexTensor.dispose();
      }
    } finally {
      boxes2d.dispose();
    }

    const objects: DetectedObject[] = [];
    for (let i = 0; i < indexes.length; i++) {
      const k = indexes[i];
      // Boxes come back as (yMin, xMin, yMax, xMax) normalized to 0..1.
      const minY = boxes[k * 4] * height;
      const minX = boxes[k * 4 + 1] * width;
      const maxY = boxes[k * 4 + 2] * height;
      const maxX = boxes[k * 4 + 3] * width;
      objects.push({
        bbox: [minX, minY, maxX - minX, maxY - minY],
        class: CLASSES[classes[k] + 1].displayName,
        score: maxScores[k],
      });
    }
    return objects;
  };

  return { detect, dispose: () => model.dispose() };
};
