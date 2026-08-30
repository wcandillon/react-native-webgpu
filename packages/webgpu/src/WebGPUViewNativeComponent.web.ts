import React from "react";
import { StyleSheet } from "react-native";
import type { Int32 } from "react-native/Libraries/Types/CodegenTypes";
import type { ViewProps } from "react-native";

import { contextIdToId } from "./utils";

export interface NativeProps extends ViewProps {
  contextId: Int32;
  transparent: boolean;
}

// eslint-disable-next-line import/no-default-export
export default function WebGPUViewNativeComponent(
  props: NativeProps,
): React.JSX.Element {
  const { contextId, style, transparent, ...rest } = props;

  // MakeWebGPUCanvasContext sets the initial drawing-buffer size. Subsequent
  // resizes belong to the renderer so it can recreate depth/MSAA attachments
  // together; changing canvas.width/height here would invalidate its textures.
  return React.createElement("canvas", {
    ...rest,
    id: contextIdToId(contextId),
    style: {
      ...styles.view,
      ...styles.flex1,
      ...(transparent === false ? { backgroundColor: "white" } : {}),
      ...(typeof style === "object" ? style : {}),
    },
  });
}

const styles = StyleSheet.create({
  flex1: {
    flex: 1,
  },
  view: {
    alignItems: "stretch",
    backgroundColor: "transparent",
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    border: "0 solid black",
    boxSizing: "border-box",
    display: "flex",
    flexBasis: "auto",
    flexDirection: "column",
    flexShrink: 0,
    listStyle: "none",
    margin: 0,
    minHeight: 0,
    minWidth: 0,
    padding: 0,
    position: "relative",
    zIndex: 0,
  },
});
