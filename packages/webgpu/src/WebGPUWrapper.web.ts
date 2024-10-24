import type { ViewProps } from "react-native";
import _ from "lodash";
import { StyleSheet } from "react-native";
//@ts-ignore - Ignore b/c it's problematic installing react-native as a devDependency since it uses @types/react-native for its own types which don't have this method
import { unstable_createElement } from "react-native-web";
import { contextIdToId } from "./utils";
import { useEffect, useRef } from "react";

export const WebGPUWrapper = (props: ViewProps & { contextId: number }) => {
  const { contextId, style, ...rest } = props;

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

  return unstable_createElement("canvas", {
    ...rest,
    style: [styles.view, styles.flex1, style],
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
    //@ts-ignore
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
    "text-decoration": "none",
    zIndex: 0,
  },
});
