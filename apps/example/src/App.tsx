import { NavigationContainer } from "@react-navigation/native";
import { createStackNavigator } from "@react-navigation/stack";
import { GestureHandlerRootView } from "react-native-gesture-handler";

import type { Routes } from "./Route";
import { Home } from "./Home";
import { Tests } from "./Tests";
import { useAssets } from "./components/useAssets";
import {
  Cube,
  TexturedCube,
  FractalCube,
  InstancedCube,
  Cubemap,
} from "./Cube";
import { HelloTriangle, HelloTriangleMSAA } from "./Triangle";
import { RenderBundles } from "./RenderBundles";
import { ABuffer } from "./ABuffer";
import { OcclusionQuery } from "./OcclusionQuery";
import { ComputeBoids } from "./ComputeBoids";
import { MNISTInference } from "./MNISTInference";
import { Wireframe } from "./Wireframe";
import { Resize } from "./Resize";
import { Particules } from "./Particles";
import { DeferedRendering, ShadowMapping } from "./ShadowMapping";
import { SamplerParameters } from "./Sampler";
import { ReversedZ } from "./ReversedZ";
import { ThreeJS } from "./ThreeJS";
import { GradientTiles } from "./GradientTiles";
import { CanvasAPI } from "./CanvasAPI";
import { Tensorflow } from "./Tensorflow";

// The two lines below are needed by three.js
import "fast-text-encoding";
window.parent = window;

const Stack = createStackNavigator<Routes>();

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
          <Stack.Screen name="ThreeJS" component={ThreeJS} />
          <Stack.Screen name="Tensorflow" component={Tensorflow} />
          <Stack.Screen name="CanvasAPI" component={CanvasAPI} />
          <Stack.Screen name="Cube" component={Cube} />
          <Stack.Screen name="InstancedCube" component={InstancedCube} />
          <Stack.Screen name="TexturedCube" component={TexturedCube} />
          <Stack.Screen name="FractalCube" component={FractalCube} />
          <Stack.Screen name="Cubemap" component={Cubemap} />
          <Stack.Screen
            name="SamplerParameters"
            component={SamplerParameters}
          />
          <Stack.Screen name="RenderBundles" component={RenderBundles} />
          <Stack.Screen name="ReversedZ" component={ReversedZ} />
          <Stack.Screen name="ABuffer" component={ABuffer} />
          <Stack.Screen name="OcclusionQuery" component={OcclusionQuery} />
          <Stack.Screen name="MNISTInference" component={MNISTInference} />
          <Stack.Screen name="ComputeBoids" component={ComputeBoids} />
          <Stack.Screen name="ShadowMapping" component={ShadowMapping} />
          <Stack.Screen name="DeferedRendering" component={DeferedRendering} />
          <Stack.Screen name="Wireframe" component={Wireframe} />
          <Stack.Screen name="Particles" component={Particules} />
          <Stack.Screen name="Resize" component={Resize} />
          <Stack.Screen name="Tests">
            {(props) => <Tests {...props} assets={assets} />}
          </Stack.Screen>
          <Stack.Screen name="GradientTiles" component={GradientTiles} />
        </Stack.Navigator>
      </NavigationContainer>
    </GestureHandlerRootView>
  );
}

// eslint-disable-next-line import/no-default-export
export default App;
