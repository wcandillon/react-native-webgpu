import type * as Tf from "@tensorflow/tfjs";

export const VOCAB_SIZE = 5000;
export const MAX_LENGTH = 32;
const EPOCHS = 50;

const SAMPLE_TEXTS = [
  "this is great",
  "i love it",
  "amazing experience",
  "i am happy",
  "feeling wonderful",
  "excellent service",
  "fantastic results",
  "very satisfied",
  "perfect solution",
  "awesome day",
  "really pleased",
  "joy and happiness",
  "feeling blessed",
  "incredible experience",
  "super excited",
  "this is fun",
  "terrible service",
  "worst product ever",
  "very disappointed",
  "i am sad",
  "feeling depressed",
  "awful experience",
  "this is horrible",
  "completely frustrated",
  "waste of time",
  "very unhappy",
  "absolutely terrible",
  "poor quality",
  "extremely disappointed",
  "total failure",
  "not satisfied",
  "this is tedious",
];

const LABELS = [
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
];

export function tokenize(text: string) {
  return text
    .toLowerCase()
    .split(/\s+/)
    .filter(Boolean)
    .map((word) => {
      const hash = Array.from(word).reduce(
        (h, char) => (h << 5) - h + char.charCodeAt(0),
        0,
      );
      return Math.abs(hash) % VOCAB_SIZE;
    });
}

export function padSequence(sequence: number[], maxLength: number) {
  if (sequence.length > maxLength) {
    return sequence.slice(0, maxLength);
  }
  return [...sequence, ...new Array(maxLength - sequence.length).fill(0)];
}

function createModel(tf: typeof Tf) {
  const model = tf.sequential();
  model.add(
    tf.layers.embedding({
      inputDim: VOCAB_SIZE,
      outputDim: 32,
      inputLength: MAX_LENGTH,
      maskZero: true,
    }),
  );
  model.add(tf.layers.globalAveragePooling1d());
  model.add(tf.layers.dense({ units: 16, activation: "relu" }));
  model.add(tf.layers.dense({ units: 1, activation: "sigmoid" }));
  model.compile({
    optimizer: tf.train.adam(0.0005),
    loss: "binaryCrossentropy",
    metrics: ["accuracy"],
  });
  return model;
}

export async function initSentimentModel(
  tf: typeof Tf,
  onEpoch: (epoch: number, total: number) => void,
) {
  await tf.setBackend("webgpu");
  await tf.ready();

  const backend = tf.getBackend();
  const model = createModel(tf);

  const sequences = SAMPLE_TEXTS.map((text) =>
    padSequence(tokenize(text), MAX_LENGTH),
  );
  const xTrain = tf.tensor2d(sequences);
  const yTrain = tf.tensor1d(LABELS);

  await model.fit(xTrain, yTrain, {
    epochs: EPOCHS,
    verbose: 0,
    callbacks: {
      onEpochEnd: (epoch) => onEpoch(epoch + 1, EPOCHS),
    },
  });

  await tf.nextFrame();

  xTrain.dispose();
  yTrain.dispose();

  return { model, backend };
}

export async function predictSentiment(
  tf: typeof Tf,
  model: Tf.LayersModel,
  text: string,
) {
  const sequence = padSequence(tokenize(text), MAX_LENGTH);
  const input = tf.tensor2d([sequence]);
  const output = model.predict(input);
  if (Array.isArray(output)) {
    input.dispose();
    throw new Error("Unexpected multi-output model");
  }
  const values = await output.data();
  output.dispose();
  input.dispose();
  return values[0] ?? 0.5;
}
