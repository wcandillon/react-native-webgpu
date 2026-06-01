import React from "react";
import { createStackNavigator } from "@react-navigation/stack";

import type { Routes } from "./Routes";
import { List } from "./List";
import { UIThread } from "./Reanimated";
import { WorkletBootstrap } from "./WorkletBootstrap";

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
        name="WorkletBootstrap"
        component={WorkletBootstrap}
        options={{
          title: "🧱 Worklet Bootstrap",
        }}
      />
    </Stack.Navigator>
  );
};
