import React from "react";
import { createNativeStackNavigator } from "@react-navigation/native-stack";

import { Cube } from "./Cube";
import type { Routes } from "./Routes";
import { List } from "./List";
import { Helmet } from "./Helmet";
import { Backdrop } from "./Backdrop";
import { Cubemap } from "./Cubemap";
import { InstancedMesh } from "./InstancedMesh";

const Stack = createNativeStackNavigator<Routes>();
export const ThreeJS = () => {
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
