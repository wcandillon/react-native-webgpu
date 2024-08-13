import { NavigationContainer } from "@react-navigation/native";
import { createNativeStackNavigator } from "@react-navigation/native-stack";
import { GestureHandlerRootView } from "react-native-gesture-handler";

import type { Routes } from "./Route";
import { Home } from "./Home";
import { Tests } from "./Tests";
import { useAssets } from "./components/useAssets";
import { Cube, TexturedCube, FractalCube, InstancedCube } from "./Cube";
import { HelloTriangle, HelloTriangleMSAA } from "./Triangle";
import { RenderBundles } from "./RenderBundles";
import { ABuffer } from "./ABuffer";
import { OcclusionQuery } from "./OcclusionQuery";
import { ComputeBoids } from "./ComputeBoids";
import { Wireframe } from "./Wireframe";
import { Resize } from "./Resize";
import { Particules } from "./Particles";
import { DeferedRendering, ShadowMapping } from "./ShadowMapping";

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
          <Stack.Screen name="InstancedCube" component={InstancedCube} />
          <Stack.Screen name="TexturedCube">
            {(props) => <TexturedCube {...props} assets={assets} />}
          </Stack.Screen>
          <Stack.Screen name="FractalCube" component={FractalCube} />
          <Stack.Screen name="RenderBundles">
            {(props) => <RenderBundles {...props} assets={assets} />}
          </Stack.Screen>
          <Stack.Screen name="ABuffer" component={ABuffer} />
          <Stack.Screen name="OcclusionQuery" component={OcclusionQuery} />
          <Stack.Screen name="ComputeBoids" component={ComputeBoids} />
          <Stack.Screen name="ShadowMapping" component={ShadowMapping} />
          <Stack.Screen name="DeferedRendering" component={DeferedRendering} />
          <Stack.Screen name="Wireframe" component={Wireframe} />
          <Stack.Screen name="Particles">
            {(props) => <Particules {...props} assets={assets} />}
          </Stack.Screen>
          <Stack.Screen name="Resize" component={Resize} />
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
