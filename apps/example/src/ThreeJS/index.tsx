import React, { useEffect } from "react";
import { createNativeStackNavigator } from "@react-navigation/native-stack";
import { warnIfNotHardwareAccelerated } from "react-native-wgpu";

import { Cube } from "./Cube";
import type { Routes } from "./Routes";
import { List } from "./List";
import { Helmet } from "./Helmet";
import { Backdrop } from "./Backdrop";
import { InstancedMesh } from "./InstancedMesh";

const Stack = createNativeStackNavigator<Routes>();
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
        name="Helmet"
        component={Helmet}
        options={{
          title: "⛑️ Helmet",
        }}
      />
    </Stack.Navigator>
  );
};
