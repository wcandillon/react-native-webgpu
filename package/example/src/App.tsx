import { NavigationContainer } from "@react-navigation/native";
import { createNativeStackNavigator } from "@react-navigation/native-stack";
import { GestureHandlerRootView } from "react-native-gesture-handler";

import type { Routes } from "./Route";
import { Home } from "./Home";
import { Tests } from "./Tests";
import { useAssets } from "./components/useAssets";
import { Cube } from "./Cube";
import { HelloTriangle, HelloTriangleMSAA } from "./Triangle";

const Stack = createNativeStackNavigator<Routes>();

function App() {
  const assets = useAssets();
  if (assets === null) {
    return null;
  }
  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <NavigationContainer>
        <Stack.Navigator initialRouteName="Home">
          <Stack.Screen name="Home" component={Home} />
          <Stack.Screen name="HelloTriangle" component={HelloTriangle} />
          <Stack.Screen
            name="HelloTriangleMSAA"
            component={HelloTriangleMSAA}
          />
          <Stack.Screen name="Cube" component={Cube} />
          <Stack.Screen name="Tests">
            {(props) => <Tests {...props} assets={assets} />}
          </Stack.Screen>
        </Stack.Navigator>
      </NavigationContainer>
    </GestureHandlerRootView>
  );
}

// eslint-disable-next-line import/no-default-export
export default App;
