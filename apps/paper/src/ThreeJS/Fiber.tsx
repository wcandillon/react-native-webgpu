import { View } from "react-native";

import { FiberCanvas } from "./components/FiberCanvas";

export const Fiber = () => {
  return (
    <View style={{ flex: 1 }}>
      <FiberCanvas style={{ flex: 1 }}>
        <color attach="background" args={["cyan"]} />
      </FiberCanvas>
    </View>
  );
};
