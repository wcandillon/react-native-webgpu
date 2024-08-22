import React from "react";
import { createNativeStackNavigator } from "@react-navigation/native-stack";

import { Cube } from "./Cube";
import type { Routes } from "./Routes";
import { List } from "./List";

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
          header: () => null,
        }}
      />
    </Stack.Navigator>
  );
};
