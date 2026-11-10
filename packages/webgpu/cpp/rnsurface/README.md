# Shared with React Native Skia

The files in this directory are copies of `packages/skia/cpp/rnsurface/` in
[react-native-skia](https://github.com/wcandillon/react-native-skia), byte for
byte, namespace included. They are the code the two libraries share around the
platform view a canvas presents into: the frame scheduler that decides when a
present thread presents onto a `SurfaceView`, a `TextureView` or a
`CAMetalLayer` (`FrameScheduler`), and the thread contract checked at the
entries that belong to that thread (`ThreadChecker`).

Change a file in react-native-skia first, where it was written (its host tests
are `packages/skia/cpp-tests/FrameSchedulerTest.cpp` and
`ThreadCheckerTest.cpp`, copied under `tests/cpp` here), then copy it over. A
diff between the two directories is the check that nothing drifted.
