import { View, TextInput, Text, Button } from "react-native";
import React, { useState, useEffect } from "react";
import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";
import { Platform } from "@tensorflow/tfjs-core";
import { GPUOffscreenCanvas } from "./OffscreenCanvas";

global.OffscreenCanvas = GPUOffscreenCanvas;

// Platform implementation for React Native
class PlatformReactNative implements Platform {
  async fetch(path, init, options) {
    return fetch(path, init, options);
  }

  encode(text, encoding) {
    if (encoding === "utf-16") {
      encoding = "utf16le";
    }
    return new Uint8Array(Buffer.from(text, encoding));
  }

  decode(bytes, encoding) {
    if (encoding === "utf-16") {
      encoding = "utf16le";
    }
    return Buffer.from(bytes).toString(encoding);
  }

  now() {
    if (global.nativePerformanceNow) {
      return global.nativePerformanceNow();
    }
    return Date.now();
  }

  setTimeoutCustom() {
    throw new Error("react native does not support setTimeoutCustom");
  }

  isTypedArray(a) {
    return (
      a instanceof Float32Array ||
      a instanceof Int32Array ||
      a instanceof Uint8Array ||
      a instanceof Uint8ClampedArray
    );
  }
}

tf.setPlatform("react-native", new PlatformReactNative());

// Vocabulary for text tokenization
const VOCAB_SIZE = 5000;
const MAX_LENGTH = 100;

// Create a simple tokenizer
const tokenize = (text) => {
  const words = text.toLowerCase().split(/\s+/);
  return words.map(word => {
    // Simple hash function for word to index mapping
    const hash = Array.from(word).reduce(
      (hash, char) => ((hash << 5) - hash) + char.charCodeAt(0),
      0
    );
    return Math.abs(hash) % VOCAB_SIZE;
  });
};

const padSequence = (sequence, maxLength) => {
  if (sequence.length > maxLength) {
    return sequence.slice(0, maxLength);
  }
  return [...sequence, ...new Array(maxLength - sequence.length).fill(0)];
};

const createModel = () => {
  const model = tf.sequential();
  
  model.add(tf.layers.embedding({
    inputDim: VOCAB_SIZE,
    outputDim: 32,
    inputLength: MAX_LENGTH
  }));
  
  model.add(tf.layers.globalAveragePooling1d());
  
  model.add(tf.layers.dense({
    units: 16,
    activation: 'relu'
  }));
  
  model.add(tf.layers.dense({
    units: 1,
    activation: 'sigmoid'
  }));
  
  model.compile({
    optimizer: tf.train.adam(0.001),
    loss: 'binaryCrossentropy',
    metrics: ['accuracy']
  });
  
  return model;
};

export const TransformerJS = () => {
  const [isModelReady, setIsModelReady] = useState(false);
  const [inputText, setInputText] = useState("");
  const [sentiment, setSentiment] = useState(null);
  const [model, setModel] = useState(null);

  useEffect(() => {
    const initializeTensorFlow = async () => {
      try {
        console.log("Initializing TensorFlow.js with WebGPU backend...");
        await tf.setBackend("webgpu");
        console.log("Backend initialized:", tf.getBackend());
        
        // Create and initialize the model
        const sentimentModel = createModel();
        
        // Train the model with some sample data
        const sampleTexts = [
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
          
          // Negative examples
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
          "not satisfied"
        ];
        
        const labels = [
          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // Positive labels
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // Negative labels
        ];
        
        // Tokenize and pad the sample texts
        const sequences = sampleTexts.map(text => 
          padSequence(tokenize(text), MAX_LENGTH)
        );
        
        // Convert to tensors
        const xTrain = tf.tensor2d(sequences);
        const yTrain = tf.tensor1d(labels);
        
        // Train the model
        await sentimentModel.fit(xTrain, yTrain, {
          epochs: 10,
          verbose: 1
        });
        
        setModel(sentimentModel);
        setIsModelReady(true);
        
        // Clean up tensors
        xTrain.dispose();
        yTrain.dispose();
        
      } catch (error) {
        console.error("Error initializing TensorFlow:", error);
      }
    };

    initializeTensorFlow();
  }, []);

  const analyzeSentiment = async () => {
    if (!model || !inputText) return;

    try {
      // Tokenize and pad the input text
      const sequence = padSequence(tokenize(inputText), MAX_LENGTH);
      
      // Convert to tensor and get prediction
      const inputTensor = tf.tensor2d([sequence]);
      const prediction = await model.predict(inputTensor).data();
      
      // Get sentiment score (0-1)
      console.log("Prediction:", prediction);
      const sentimentScore = prediction[0];
      setSentiment(sentimentScore);
      
      // Clean up tensor
      inputTensor.dispose();
      
    } catch (error) {
      console.error("Error analyzing sentiment:", error);
    }
  };

  return (
    <View style={{ flex: 1, padding: 20 }}>
      <Text style={{ fontSize: 24, marginBottom: 20 }}>
        Sentiment Analysis Demo
      </Text>
      
      {!isModelReady ? (
        <Text>Loading model...</Text>
      ) : (
        <>
          <TextInput
            style={{
              height: 100,
              borderColor: 'gray',
              borderWidth: 1,
              marginBottom: 20,
              padding: 10
            }}
            multiline
            placeholder="Enter text to analyze..."
            value={inputText}
            onChangeText={setInputText}
          />
          
          <Button
            title="Analyze Sentiment"
            onPress={analyzeSentiment}
            disabled={!inputText}
          />
          
          {sentiment !== null && (
            <View style={{ marginTop: 20 }}>
              <Text style={{ fontSize: 18 }}>
                Sentiment Score: {sentiment.toFixed(6)}
              </Text>
              <Text style={{ fontSize: 16, marginTop: 10 }}>
                {sentiment > 0.5 ? "Positive 😊" : "Negative 😞"}
              </Text>
            </View>
          )}
        </>
      )}
    </View>
  );
};