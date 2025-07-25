import _ from "lodash";
import { useEffect, useRef } from "react";
import { StyleSheet } from "react-native";
// @ts-expect-error - rn web uses @types/react-native and doesn't have types for web only exports
// eslint-disable-next-line import/no-extraneous-dependencies
import { unstable_createElement as unstableCreateElement } from "react-native-web";

import { contextIdToId } from "./utils";
import type { NativeProps } from "./WebGPUViewNativeComponent";

export const WebGPUWrapper = (props: NativeProps) => {
  const { contextId, style, transparent, ...rest } = props;

  const canvasElm = useRef<HTMLCanvasElement>();

  useEffect(() => {
    const onResize = _.debounce(() => resizeCanvas(canvasElm.current), 100, {
      leading: false,
      trailing: true,
      maxWait: 2000,
    });
    window.addEventListener("resize", onResize);
    return () => {
      window.removeEventListener("resize", onResize);
    };
  }, []);

  return unstableCreateElement("canvas", {
    ...rest,
    style: [
      styles.view,
      styles.flex1,
      transparent === false && { backgroundColor: "white" }, // Canvas elements are transparent by default on the web
      style,
    ],
    id: contextIdToId(contextId),
    ref: (ref: HTMLCanvasElement) => {
      canvasElm.current = ref;
      if (ref) {
        resizeCanvas(ref);
      }
    },
  });
};

function resizeCanvas(canvas?: HTMLCanvasElement) {
  if (!canvas) {
    return;
  }

  const dpr = window.devicePixelRatio || 1;

  const { height, width } = canvas.getBoundingClientRect();
  canvas.setAttribute("height", (height * dpr).toString());
  canvas.setAttribute("width", (width * dpr).toString());
}

const styles = StyleSheet.create({
  flex1: {
    flex: 1,
  },
  view: {
    alignItems: "stretch",
    backgroundColor: "transparent",
    //@ts-expect-error need a web only override
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
