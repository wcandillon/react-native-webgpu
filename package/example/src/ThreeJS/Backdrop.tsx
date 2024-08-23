import * as THREE from "three/webgpu";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import { View } from "react-native";
import { useRef } from "react";
import { GLTFLoader } from "three-stdlib";

import { useCanvasEffect } from "../components/useCanvasEffect";

import { manager } from "./assets/AssetManager";

const {
  float,
  vec3,
  color,
  viewportSharedTexture,
  viewportTopLeft,
  checker,
  uv,
  timerLocal,
  oscSine,
  output,
} = THREE;

export const Backdrop = () => {
  const ref = useRef<CanvasRef>(null);
  useCanvasEffect(async ({ device }) => {
    const rotate = true;
    const context = ref.current!.getContext("webgpu")!;
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
      scene.backgroundNode = viewportTopLeft.y.mix(
        color(0x66bbff),
        color(0x4466ff),
      );
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

        const { material } = object.children[0].children[0];

        // output material effect ( better using hsv )
        // ignore output.sRGBToLinear().linearTosRGB() for now

        material.outputNode = oscSine(timerLocal(0.1)).mix(
          output,
          output.add(0.1).posterize(4).mul(2),
        );

        const action = mixer.clipAction(gltf.animations[0]);
        action.play();

        scene.add(object);
      });

      // portals

      const geometry = new THREE.SphereGeometry(0.3, 32, 16);

      portals = new THREE.Group();
      scene.add(portals);

      function addBackdropSphere(backdropNode, backdropAlphaNode = null) {
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
        viewportSharedTexture().bgr.hue(oscSine().mul(Math.PI)),
      );
      addBackdropSphere(viewportSharedTexture().rgb.oneMinus());
      addBackdropSphere(viewportSharedTexture().rgb.saturation(0));
      addBackdropSphere(viewportSharedTexture().rgb.saturation(10), oscSine());
      addBackdropSphere(
        viewportSharedTexture().rgb.overlay(checker(uv().mul(10))),
      );
      addBackdropSphere(
        viewportSharedTexture(viewportTopLeft.mul(40).floor().div(40)),
      );
      addBackdropSphere(
        viewportSharedTexture(viewportTopLeft.mul(80).floor().div(80)).add(
          color(0x0033ff),
        ),
      );
      addBackdropSphere(vec3(0, 0, viewportSharedTexture().b));

      //renderer

      renderer = new THREE.WebGPURenderer({
        antialias: true,
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-expect-error
        canvas: context.canvas,
        context,
        device,
      });
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
