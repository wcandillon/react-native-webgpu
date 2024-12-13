import { View } from "react-native";

// Import TensorFlow.js and required modules
import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";
//import * as tfvis from "@tensorflow/tfjs-vis";

(async () => {
  // Set the backend to WebGPU
  const result = await tf.setBackend("webgpu");
  console.log("WebGPU backend is ready: " + result);

  // // Load and preprocess the Fashion MNIST dataset
  // const loadFashionMnist = async () => {
  //   const mnist = await tf.data.csv(
  //     "https://storage.googleapis.com/tfjs-examples/mnist-data/mnist_test.csv",
  //     { columnConfigs: { label: { isLabel: true } } },
  //   );

  //   const NUM_CLASSES = 10;
  //   const DATA_SIZE = 70000;

  //   // Split features and labels, normalize feature values
  //   const processedData = mnist.map(({ xs, ys }) => {
  //     const labels = tf.oneHot(tf.tensor1d([ys.label], "int32"), NUM_CLASSES);
  //     const images = tf.tensor2d(Object.values(xs), [1, 28 * 28]).div(255);
  //     return { xs: images, ys: labels };
  //   });

  //   const split = DATA_SIZE * 0.8;
  //   return {
  //     trainData: processedData.take(split),
  //     testData: processedData.skip(split),
  //   };
  // };

  // // Define a simple CNN model
  // const createModel = () => {
  //   const model = tf.sequential();

  //   // Add layers
  //   model.add(
  //     tf.layers.conv2d({
  //       inputShape: [28, 28, 1],
  //       kernelSize: 3,
  //       filters: 16,
  //       activation: "relu",
  //     }),
  //   );
  //   model.add(tf.layers.maxPooling2d({ poolSize: [2, 2] }));
  //   model.add(tf.layers.flatten());
  //   model.add(tf.layers.dense({ units: 128, activation: "relu" }));
  //   model.add(tf.layers.dense({ units: 10, activation: "softmax" }));

  //   model.compile({
  //     optimizer: tf.train.adam(),
  //     loss: "categoricalCrossentropy",
  //     metrics: ["accuracy"],
  //   });

  //   return model;
  // };

  // // Train and evaluate the model
  // const trainModel = async (model, trainData, testData) => {
  //   const BATCH_SIZE = 32;
  //   const EPOCHS = 5;

  //   const trainXs = trainData.map(({ xs }) => xs);
  //   const trainYs = trainData.map(({ ys }) => ys);

  //   const testXs = testData.map(({ xs }) => xs);
  //   const testYs = testData.map(({ ys }) => ys);

  //   await model.fit(trainXs, trainYs, {
  //     batchSize: BATCH_SIZE,
  //     epochs: EPOCHS,
  //     validationData: [testXs, testYs],
  //     callbacks: tfvis.show.fitCallbacks(
  //       { name: "Training Performance" },
  //       ["loss", "val_loss", "accuracy", "val_accuracy"],
  //       { height: 200, callbacks: ["onEpochEnd"] },
  //     ),
  //   });

  //   const testResult = model.evaluate(testXs, testYs);
  //   console.log("Test Accuracy:", testResult[1].dataSync()[0]);
  // };

  // (async () => {
  //   const { trainData, testData } = await loadFashionMnist();
  //   const model = createModel();
  //   await trainModel(model, trainData, testData);
  // })();
})();

export const TransformerJS = () => {
  return <View style={{ flex: 1, backgroundColor: "cyan" }} />;
};
