import React, { useEffect } from "react";
import { createStackNavigator } from "@react-navigation/stack";
import { warnIfNotHardwareAccelerated } from "react-native-webgpu";

import { Cube } from "./Cube";
import type { Routes } from "./Routes";
import { List } from "./List";
import { Helmet } from "./Helmet";
import { Backdrop } from "./Backdrop";
import { InstancedMesh } from "./InstancedMesh";
import { Fiber } from "./Fiber";
import { PostProcessing } from "./PostProcessing";
import { Retargeting } from "./Retargeting";

const Stack = createStackNavigator<Routes>();
export const ThreeJS = () => {
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
          title: "🧊 Three.js",
          header: () => null,
        }}
      />
      <Stack.Screen
        name="Fiber"
        component={Fiber}
        options={{
          title: "👕 Fiber",
        }}
      />
      <Stack.Screen
        name="Cube"
        component={Cube}
        options={{
          title: "🧊 Cube",
        }}
      />
      <Stack.Screen
        name="Backdrop"
        component={Backdrop}
        options={{
          title: "💃 Backdrop",
        }}
      />
      <Stack.Screen
        name="InstancedMesh"
        component={InstancedMesh}
        options={{
          title: "🐵 InstancedMesh",
        }}
      />
      <Stack.Screen
        name="PostProcessing"
        component={PostProcessing}
        options={{
          title: "🪄 Post Processing Effects",
        }}
      />
      <Stack.Screen
        name="Helmet"
        component={Helmet}
        options={{
          title: "⛑️ Helmet",
        }}
      />
      <Stack.Screen
        name="Retargeting"
        component={Retargeting}
        options={{
          title: "🕺 Animation Retargeting",
        }}
      />
    </Stack.Navigator>
  );
};
