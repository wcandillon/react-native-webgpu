import * as React from "react";
import { NavigationContainer } from "@react-navigation/native";
import { createNativeStackNavigator } from "@react-navigation/native-stack";
import { Routes } from "./Route";
import { Home } from "./Home";
import { Buffers } from './Buffers';
import { Tests } from "./Tests";
import { GestureHandlerRootView } from "react-native-gesture-handler";


const Stack = createNativeStackNavigator<Routes>();

function App() {
  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <NavigationContainer>
        <Stack.Navigator>
          <Stack.Screen name="Home" component={Home} />
          <Stack.Screen name="Buffers" component={Buffers} />
          <Stack.Screen name="Tests" component={Tests} />
        </Stack.Navigator>
      </NavigationContainer>
    </GestureHandlerRootView>
  );
}

// eslint-disable-next-line import/no-default-export
export default App;
