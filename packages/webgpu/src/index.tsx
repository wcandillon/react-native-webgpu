/// <reference types="@webgpu/types" />

export * from "./main";

declare global {
  interface Navigator {
    gpu: GPU;
  }

  // If a non-DOM env, this augment global with navigator
  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
  // @ts-ignore: Ignore if 'Navigator' doesn't exist
  // eslint-disable-next-line no-var
  var navigator: Navigator;
}
