import React, { useEffect } from "react";
import { createNativeStackNavigator } from "@react-navigation/native-stack";
import { warnIfNotHardwareAccelerated } from "react-native-wgpu";

import { GradientTiles } from "./GradientTiles";
import type { Routes } from "./Routes";
import { List } from "./List";
import { Boids } from "./Boids";

const Stack = createNativeStackNavigator<Routes>();
export const TypeGPU = () => {
  useEffect(() => {
    navigator.gpu.requestAdapter().then((adapter) => {
      if (adapter) {
        warnIfNotHardwareAccelerated(adapter);
      }
    });
  }, []);
  return (
    <Stack.Navigator>
      <Stack.Screen
        name="List"
        component={List}
        options={{
          title: "💜 TypeGPU",
          header: () => null,
        }}
      />
      <Stack.Screen
        name="GradientTiles"
        component={GradientTiles}
        options={{
          title: "🌈 Gradient Tiles",
        }}
      />
      <Stack.Screen
        name="Boids"
        component={Boids}
        options={{
          title: "🐧 Boids",
        }}
      />
    </Stack.Navigator>
  );
};
