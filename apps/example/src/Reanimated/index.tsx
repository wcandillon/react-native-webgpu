import React from "react";
import { createStackNavigator } from "@react-navigation/stack";

import type { Routes } from "./Routes";
import { List } from "./List";
import { UIThread } from "./UIThread";
import { DedicatedThread } from "./DedicatedThread";
import { FrameProcessor } from "./FrameProcessor";

const Stack = createStackNavigator<Routes>();
export const Reanimated = () => {
  return (
    <Stack.Navigator>
      <Stack.Screen
        name="List"
        component={List}
        options={{
          title: "🎬 Reanimated",
          header: () => null,
        }}
      />
      <Stack.Screen
        name="UIThread"
        component={UIThread}
        options={{
          title: "🧵 UI Thread",
        }}
      />
      <Stack.Screen
        name="DedicatedThread"
        component={DedicatedThread}
        options={{
          title: "🔀 Dedicated Thread",
        }}
      />
      <Stack.Screen
        name="FrameProcessor"
        component={FrameProcessor}
        options={{
          title: "📷 Frame Processor",
        }}
      />
    </Stack.Navigator>
  );
};
