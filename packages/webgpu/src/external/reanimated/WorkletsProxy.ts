import type * as WorkletsT from "react-native-worklets";

import {
  OptionalDependencyNotInstalledError,
  createModuleProxy,
} from "../ModuleProxy";

type TWorklets = typeof WorkletsT;

const Worklets = createModuleProxy<TWorklets>(() => {
  try {
    return require("react-native-worklets");
  } catch (e) {
    throw new OptionalDependencyNotInstalledError("react-native-worklets");
  }
});

// eslint-disable-next-line import/no-default-export
export default Worklets;
