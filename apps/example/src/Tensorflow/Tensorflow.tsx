/* eslint-disable prefer-destructuring */

import { View, TextInput, Text, Button, ActivityIndicator } from "react-native";
import React, { useState, useEffect } from "react";
import * as tf from "@tensorflow/tfjs";

import "@tensorflow/tfjs-backend-webgpu";
import { PlatformReactNative } from "./Platform";

tf.setPlatform("react-native", new PlatformReactNative());

// Constants for the pre-trained model
const MODEL_URL =
  "https://storage.googleapis.com/tfjs-models/tfjs/sentiment_cnn_v1/model.json";
const METADATA_URL =
  "https://storage.googleapis.com/tfjs-models/tfjs/sentiment_cnn_v1/metadata.json";
const OOV_INDEX = 2; // Out-of-vocabulary word index for this model

// Type for the model metadata
interface ModelMetadata {
  word_index: Record<string, number>;
  vocabulary_size: number;
  max_len: number;
  index_from: number;
}

// Tokenizer for the pre-trained model
class PretrainedTokenizer {
  wordIndex: Record<string, number> = {};
  vocabularySize: number;
  maxLen: number;
  indexFrom: number;

  constructor(metadata: ModelMetadata) {
    this.wordIndex = metadata.word_index;
    this.vocabularySize = metadata.vocabulary_size;
    this.maxLen = metadata.max_len;
    this.indexFrom = metadata.index_from;
  }

  textToSequence(text: string): number[] {
    // Convert the text to lowercase and split by spaces
    const words = text.toLowerCase().split(/\s+/);
    // Map each word to its index in the vocabulary
    return words.map((word) => {
      // Use the word index if available, otherwise use OOV index
      return this.wordIndex[word] + this.indexFrom || OOV_INDEX;
    });
  }
}

// Pad sequence to a fixed length
const padSequence = (sequence: number[], maxLen: number) => {
  if (sequence.length > maxLen) {
    return sequence.slice(0, maxLen);
  }
  return [...sequence, ...new Array(maxLen - sequence.length).fill(0)];
};

export const Tensorflow = () => {
  const [isModelReady, setIsModelReady] = useState(false);
  const [isLoading, setIsLoading] = useState(true);
  const [loadingStatus, setLoadingStatus] = useState(
    "Initializing TensorFlow.js...",
  );
  const [inputText, setInputText] = useState("");
  const [sentiment, setSentiment] = useState<null | number>(null);
  const [model, setModel] = useState<null | tf.LayersModel>(null);
  const [tokenizer, setTokenizer] = useState<null | PretrainedTokenizer>(null);

  useEffect(() => {
    const initializeTensorFlow = async () => {
      try {
        setLoadingStatus("Initializing TensorFlow.js with WebGPU backend...");
        await tf.setBackend("webgpu");
        console.log("Backend initialized:", tf.getBackend());

        // Load the pre-trained model metadata
        setLoadingStatus("Loading model metadata...");
        const metadataResponse = await fetch(METADATA_URL);
        const metadata = (await metadataResponse.json()) as ModelMetadata;
        const newTokenizer = new PretrainedTokenizer(metadata);
        setTokenizer(newTokenizer);

        // Load the pre-trained model
        setLoadingStatus("Loading pre-trained model...");
        const sentimentModel = await tf.loadLayersModel(MODEL_URL);
        console.log("Model loaded successfully");

        // Warm up the model with a sample prediction
        setLoadingStatus("Warming up model...");
        const dummySequence = padSequence([0], metadata.max_len);
        const dummyTensor = tf.tensor2d([dummySequence]);
        const predictionResult = sentimentModel.predict(dummyTensor);

        if (predictionResult instanceof tf.Tensor) {
          await predictionResult.data();
          predictionResult.dispose();
        }
        dummyTensor.dispose();

        setModel(sentimentModel);
        setIsModelReady(true);
        setIsLoading(false);
      } catch (error: unknown) {
        console.error("Error initializing TensorFlow:", error);
        setLoadingStatus(
          `Error: ${error instanceof Error ? error.message : String(error)}`,
        );
        setIsLoading(false);
      }
    };

    initializeTensorFlow();
  }, []);

  const analyzeSentiment = async () => {
    if (!model || !inputText || !tokenizer) {
      return;
    }

    try {
      // Show loading state during prediction
      setIsLoading(true);
      setSentiment(null);

      // Tokenize and pad the input text
      const sequence = tokenizer.textToSequence(inputText);
      const paddedSequence = padSequence(sequence, tokenizer.maxLen);

      // Convert to tensor and get prediction
      const inputTensor = tf.tensor2d([paddedSequence]);
      const predictionResult = model.predict(inputTensor);

      let predictionData;
      if (predictionResult instanceof tf.Tensor) {
        predictionData = await predictionResult.data();
        predictionResult.dispose();
      } else {
        // Handle array of tensors case (though unlikely in this model)
        console.warn("Prediction returned multiple tensors, using first one");
        predictionData = await predictionResult[0].data();
        predictionResult.forEach((tensor) => tensor.dispose());
      }

      // Get sentiment score (0-1)
      console.log("Prediction:", predictionData);
      const sentimentScore = predictionData[0];
      setSentiment(sentimentScore);

      // Clean up tensor
      inputTensor.dispose();
      setIsLoading(false);
    } catch (error: unknown) {
      console.error("Error analyzing sentiment:", error);
      setIsLoading(false);
    }
  };

  return (
    <View style={{ flex: 1, padding: 20 }}>
      <Text style={{ fontSize: 24, marginBottom: 20 }}>
        Sentiment Analysis Demo
      </Text>

      {!isModelReady ? (
        <View style={{ alignItems: "center", justifyContent: "center" }}>
          <ActivityIndicator size="large" color="#0000ff" />
          <Text style={{ marginTop: 10 }}>{loadingStatus}</Text>
        </View>
      ) : (
        <>
          <TextInput
            style={{
              height: 100,
              borderColor: "gray",
              borderWidth: 1,
              marginBottom: 20,
              padding: 10,
            }}
            multiline
            placeholder="Enter text to analyze..."
            value={inputText}
            onChangeText={setInputText}
          />

          <Button
            title={isLoading ? "Analyzing..." : "Analyze Sentiment"}
            onPress={analyzeSentiment}
            disabled={!inputText || isLoading}
          />

          {isLoading && (
            <View style={{ marginTop: 20, alignItems: "center" }}>
              <ActivityIndicator size="small" color="#0000ff" />
              <Text>Processing...</Text>
            </View>
          )}

          {sentiment !== null && !isLoading && (
            <View style={{ marginTop: 20 }}>
              <Text style={{ fontSize: 18 }}>
                Sentiment Score: {sentiment.toFixed(6)}
              </Text>
              <Text style={{ fontSize: 16, marginTop: 10 }}>
                {sentiment > 0.5 ? "Positive 😊" : "Negative 😞"}
              </Text>
              <View
                style={{
                  marginTop: 15,
                  height: 20,
                  backgroundColor: "#e0e0e0",
                  borderRadius: 10,
                  overflow: "hidden",
                }}
              >
                <View
                  style={{
                    width: `${sentiment * 100}%`,
                    height: 20,
                    backgroundColor: sentiment > 0.5 ? "#4CAF50" : "#F44336",
                  }}
                />
              </View>
            </View>
          )}
        </>
      )}
    </View>
  );
};
