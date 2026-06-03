---
id: canvas-transparency
title: Canvas Transparency
sidebar_position: 2
---

# Canvas Transparency

On the Web, you make a canvas transparent through the `alphaMode` you pass to `context.configure`. That works on iOS, macOS, and visionOS here too.

On **Android**, the `alphaMode` property is ignored when configuring the canvas. To get a transparent canvas on Android, use the `transparent` prop on the `<Canvas />` component instead:

```tsx
return (
  <View style={style.container}>
    <Canvas ref={ref} style={style.webgpu} transparent />
  </View>
);
```

For portable code, it is fine to set both: configure `alphaMode: "premultiplied"` (or `"opaque"`) as you would on the Web, and add `transparent` so Android behaves the same way as the other platforms.
