// @ts-nocheck

import { View } from "react-native";

// Import @tensorflow/tfjs or @tensorflow/tfjs-core
import * as tf from "@tensorflow/tfjs";
// Add the WebGPU backend to the global backend registry.
import "@tensorflow/tfjs-backend-webgpu";
// Set the backend to WebGPU and wait for the module to be ready.
tf.setBackend("webgpu").then(() => console.log("WebGPU backend is ready"));
// Listen for messages from the main thread
// self.addEventListener("message", async (e) => {
//   const { type, data } = e.data;

//   switch (type) {
//     case "check":
//       check();
//       break;

//     case "load":
//       load();
//       break;

//     case "generate":
//       stopping_criteria.reset();
//       generate(data);
//       break;

//     case "interrupt":
//       stopping_criteria.interrupt();
//       break;

//     case "reset":
//       // past_key_values_cache = null;
//       stopping_criteria.reset();
//       break;
//   }
// });

export const TransformerJS = () => {
  return <View style={{ flex: 1, backgroundColor: "cyan" }} />;
};
