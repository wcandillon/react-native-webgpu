/* eslint-disable @typescript-eslint/ban-ts-comment */
/* eslint-disable prefer-destructuring */
import * as THREE from "three";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { PixelRatio, View } from "react-native";
import { GLTFLoader } from "three-stdlib";

import { manager } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

const {
  float,
  vec3,
  color,
  viewportSharedTexture,
  checker,
  uv,
  timerLocal,
  oscSine,
  output,
  posterize,
  hue,
  grayscale,
  saturation,
  overlay,
  viewportUV,
  viewportSafeUV,
} = THREE;

export const Backdrop = () => {
  const ref = useCanvasEffect(async () => {
    const rotate = true;

    // Anti alias in the renderer is set to false and we handle the pixel density here
    const context = ref.current!.getContext("webgpu")!;
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();

    const { width, height } = context.canvas;
    let camera: THREE.Camera, scene: THREE.Scene, renderer: THREE.Renderer;
    let portals: THREE.Group;
    let mixer: THREE.AnimationMixer;
    let clock: THREE.Clock;

    init();

    function init() {
      camera = new THREE.PerspectiveCamera(50, width / height, 0.01, 100);
      camera.position.set(1, 2, 3);

      scene = new THREE.Scene();
      // @ts-expect-error
      scene.backgroundNode = viewportUV.y.mix(color(0x66bbff), color(0x4466ff));
      camera.lookAt(0, 1, 0);

      clock = new THREE.Clock();

      //lights

      const light = new THREE.SpotLight(0xffffff, 1);
      light.power = 2000;
      camera.add(light);
      scene.add(camera);

      const loader = new GLTFLoader(manager);
      loader.load("models/gltf/Michelle.glb", function (gltf) {
        const object = gltf.scene;
        mixer = new THREE.AnimationMixer(object);

        // @ts-expect-error
        const { material } = object.children[0].children[0];

        // output material effect ( better using hsv )
        // ignore output.sRGBToLinear().linearTosRGB() for now

        material.outputNode = oscSine(timerLocal(0.1)).mix(
          output,
          posterize(output.add(0.1), 4).mul(2),
        );

        const action = mixer.clipAction(gltf.animations[0]);
        action.play();

        scene.add(object);
      });

      // portals

      const geometry = new THREE.SphereGeometry(0.3, 32, 16);

      portals = new THREE.Group();
      scene.add(portals);

      function addBackdropSphere(
        backdropNode: THREE.Node,
        backdropAlphaNode = null,
      ) {
        const distance = 1;
        const id = portals.children.length;
        const rotation = THREE.MathUtils.degToRad(id * 45);

        const material = new THREE.MeshStandardNodeMaterial({
          color: 0x0066ff,
        });
        material.roughnessNode = float(0.2);
        material.metalnessNode = float(0);
        material.backdropNode = backdropNode;
        material.backdropAlphaNode = backdropAlphaNode;
        material.transparent = true;

        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.set(
          Math.cos(rotation) * distance,
          1,
          Math.sin(rotation) * distance,
        );

        portals.add(mesh);
      }

      addBackdropSphere(
        hue(viewportSharedTexture().bgr, oscSine().mul(Math.PI)),
      );
      addBackdropSphere(viewportSharedTexture().rgb.oneMinus());
      addBackdropSphere(grayscale(viewportSharedTexture().rgb));
      // @ts-expect-error
      addBackdropSphere(saturation(viewportSharedTexture().rgb, 10), oscSine());
      addBackdropSphere(
        overlay(viewportSharedTexture().rgb, checker(uv().mul(10))),
      );

      // For the two nodes below to work, antialias needs to be set to false in renderer
      // See https://github.com/mrdoob/three.js/pull/29025/files#r1753646774
      addBackdropSphere(
        viewportSharedTexture(
          viewportSafeUV(viewportUV.mul(40).floor().div(40)),
        ),
      );
      addBackdropSphere(
        viewportSharedTexture(
          viewportSafeUV(viewportUV.mul(80).floor().div(80)),
        ).add(color(0x0033ff)),
      );

      addBackdropSphere(vec3(0, 0, viewportSharedTexture().b));

      //renderer
      renderer = makeWebGPURenderer(context, { antialias: false });
      renderer.setAnimationLoop(animate);
      renderer.toneMapping = THREE.NeutralToneMapping;
      renderer.toneMappingExposure = 0.3;
    }

    function animate() {
      const delta = clock.getDelta();

      if (mixer) {
        mixer.update(delta);
      }

      if (rotate) {
        portals.rotation.y += delta * 0.5;
      }

      renderer.render(scene, camera);
      context.present();
    }
    return () => {
      renderer.setAnimationLoop(null);
    };
  });

  return (
    <View style={{ flex: 1 }}>
      <Canvas ref={ref} style={{ flex: 1 }} />
    </View>
  );
};
