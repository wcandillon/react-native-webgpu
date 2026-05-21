// Port of https://threejs.org/examples/?q=retarget#webgpu_animation_retargeting
//
// Notes on differences from the upstream demo, plus things to revisit on the
// next three.js bump (currently pinned to r184):
//
//   * `Inspector`, `OrbitControls`, helpers, `window.onresize` and the GUI
//     are intentionally omitted (no DOM / no GUI host in RN).
//   * Upstream skips `timer.connect(document)` to use the Page Visibility API;
//     we skip it because there is no `document` in RN. Page-hidden behaviour
//     is handled by Metro/RN lifecycle, not Timer.
//   * Soldier.glb embeds its two JPEG textures in the binary chunk. The RN
//     GLTFLoader can't `new Blob([ArrayBuffer])` so embedded images fail to
//     decode. We ship `assets/soldier/` as a split `.gltf` + `.bin` + two
//     `.jpg` files (same trick Michelle uses). If a future RN/Hermes gains
//     ArrayBuffer-Blob support, the original `Soldier.glb` would work
//     unchanged.
//   * `lightSpeed` is declared `vec2 -> vec3`; in r172 the example silently
//     passed `normalWorldGeometry` (a vec3) and TSL coerced it. The current
//     TS types require an explicit `.xy` swizzle - keep it on any future
//     bumps unless TSL adds implicit vec3->vec2 truncation back.
//   * `targetScene.children[0].children[0]` assumes the GLTFLoader keeps the
//     `Character -> SkinnedMesh` order from the source file. If an upgrade
//     ever changes that, switch to a `traverse` lookup with `.isSkinnedMesh`.
//   * `frustumCulled = false` on the SkinnedMeshes works around the
//     bind-pose bounding box culling the retargeted target. If a future
//     three.js update recomputes SkinnedMesh bounds after retargeting, this
//     override can go.
import * as THREE from "three";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";
import { PixelRatio, StyleSheet, Text, View } from "react-native";
import { useEffect, useRef } from "react";
import {
  Fn,
  atan,
  blendDodge,
  color,
  cos,
  float,
  hue,
  length,
  mul,
  normalWorldGeometry,
  pow,
  reflector,
  screenUV,
  sin,
  sub,
  time,
  vec2,
  vec3,
} from "three/tsl";
import * as SkeletonUtils from "three/addons/utils/SkeletonUtils";

import { useGLTF } from "./assets/AssetManager";
import { makeWebGPURenderer } from "./components/makeWebGPURenderer";

// forked from https://www.shadertoy.com/view/7ly3D1
const lightSpeed = Fn(([suvImmutable]: [THREE.Node<"vec2">]) => {
  const suv = vec2(suvImmutable);
  const uv = vec2(length(suv), atan(suv.y, suv.x));
  const offset = float(
    float(0.1)
      .mul(sin(uv.y.mul(10).sub(time.mul(0.6))))
      .mul(cos(uv.y.mul(48).add(time.mul(0.3))))
      .mul(cos(uv.y.mul(3.7).add(time))),
  );
  const rays = vec3(
    vec3(sin(uv.y.mul(150).add(time)).mul(0.5).add(0.5))
      .mul(
        vec3(
          sin(uv.y.mul(80).sub(time.mul(0.6)))
            .mul(0.5)
            .add(0.5),
        ),
      )
      .mul(
        vec3(
          sin(uv.y.mul(45).add(time.mul(0.8)))
            .mul(0.5)
            .add(0.5),
        ),
      )
      .mul(
        vec3(
          sub(
            1,
            cos(
              uv.y.add(mul(22, time).sub(pow(uv.x.add(offset), 0.3).mul(60))),
            ),
          ),
        ),
      )
      .mul(vec3(uv.x.mul(2))),
  );

  return rays;
}).setLayout({
  name: "lightSpeed",
  type: "vec3",
  inputs: [{ name: "suv", type: "vec2" }],
});

interface Source {
  clip: THREE.AnimationClip;
  skeleton: THREE.Skeleton;
  mixer: THREE.AnimationMixer;
}

function getSource(
  sourceScene: THREE.Object3D,
  clip: THREE.AnimationClip,
): Source {
  const helper = new THREE.SkeletonHelper(sourceScene);
  const skeleton = new THREE.Skeleton(helper.bones);

  const mixer = new THREE.AnimationMixer(sourceScene);
  mixer.clipAction(clip).play();

  return { clip, skeleton, mixer };
}

function retargetModel(source: Source, targetScene: THREE.Object3D) {
  const targetSkin = targetScene.children[0].children[0] as THREE.SkinnedMesh;

  const rotateCW45 = new THREE.Matrix4().makeRotationY(
    THREE.MathUtils.degToRad(45),
  );
  const rotateCCW180 = new THREE.Matrix4().makeRotationY(
    THREE.MathUtils.degToRad(-180),
  );
  const rotateCW180 = new THREE.Matrix4().makeRotationY(
    THREE.MathUtils.degToRad(180),
  );
  const rotateFoot = new THREE.Matrix4().makeRotationFromEuler(
    new THREE.Euler(
      THREE.MathUtils.degToRad(45),
      THREE.MathUtils.degToRad(180),
      THREE.MathUtils.degToRad(0),
    ),
  );

  const retargetOptions = {
    hip: "mixamorigHips",
    scale: 1 / targetScene.scale.y,
    localOffsets: {
      mixamorigLeftShoulder: rotateCW45,
      mixamorigRightShoulder: rotateCCW180,
      mixamorigLeftArm: rotateCW45,
      mixamorigRightArm: rotateCCW180,
      mixamorigLeftForeArm: rotateCW45,
      mixamorigRightForeArm: rotateCCW180,
      mixamorigLeftHand: rotateCW45,
      mixamorigRightHand: rotateCCW180,
      mixamorigLeftUpLeg: rotateCW180,
      mixamorigRightUpLeg: rotateCW180,
      mixamorigLeftLeg: rotateCW180,
      mixamorigRightLeg: rotateCW180,
      mixamorigLeftFoot: rotateFoot,
      mixamorigRightFoot: rotateFoot,
      mixamorigLeftToeBase: rotateCW180,
      mixamorigRightToeBase: rotateCW180,
    },
    names: {
      mixamorigHips: "mixamorigHips",
      mixamorigSpine: "mixamorigSpine",
      mixamorigSpine2: "mixamorigSpine2",
      mixamorigHead: "mixamorigHead",
      mixamorigLeftShoulder: "mixamorigLeftShoulder",
      mixamorigRightShoulder: "mixamorigRightShoulder",
      mixamorigLeftArm: "mixamorigLeftArm",
      mixamorigRightArm: "mixamorigRightArm",
      mixamorigLeftForeArm: "mixamorigLeftForeArm",
      mixamorigRightForeArm: "mixamorigRightForeArm",
      mixamorigLeftHand: "mixamorigLeftHand",
      mixamorigRightHand: "mixamorigRightHand",
      mixamorigLeftUpLeg: "mixamorigLeftUpLeg",
      mixamorigRightUpLeg: "mixamorigRightUpLeg",
      mixamorigLeftLeg: "mixamorigLeftLeg",
      mixamorigRightLeg: "mixamorigRightLeg",
      mixamorigLeftFoot: "mixamorigLeftFoot",
      mixamorigRightFoot: "mixamorigRightFoot",
      mixamorigLeftToeBase: "mixamorigLeftToeBase",
      mixamorigRightToeBase: "mixamorigRightToeBase",
    },
  };

  const retargetedClip = SkeletonUtils.retargetClip(
    targetSkin,
    source.skeleton,
    source.clip,
    retargetOptions,
  );

  const mixer = new THREE.AnimationMixer(targetSkin);
  mixer.clipAction(retargetedClip).play();

  return mixer;
}

export const Retargeting = () => {
  const sourceGltf = useGLTF(require("./assets/michelle/model.gltf"));
  const targetGltf = useGLTF(require("./assets/soldier/Soldier.gltf"));
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    if (!sourceGltf || !targetGltf) {
      return;
    }
    const context = ref.current?.getContext("webgpu")!;
    const canvas = context.canvas as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();

    const { width, height } = context.canvas;

    const sourceScene = sourceGltf.scene;
    const targetScene = targetGltf.scene;

    const scene = new THREE.Scene();

    // background
    const coloredVignette = screenUV
      .distance(0.5)
      .mix(
        hue(color(0x0175ad), time.mul(0.1)),
        hue(color(0x02274f), time.mul(0.5)),
      );
    const lightSpeedEffect = lightSpeed(normalWorldGeometry.xy).clamp();
    const lightSpeedSky = normalWorldGeometry.y
      .remapClamp(-0.1, 1)
      .mix(0, lightSpeedEffect);
    const composedBackground = blendDodge(coloredVignette, lightSpeedSky);
    scene.backgroundNode = composedBackground;

    const light = new THREE.HemisphereLight(0xe9c0a5, 0x0175ad, 5);
    scene.add(light);

    const dirLight = new THREE.DirectionalLight(0xfff9ea, 4);
    dirLight.position.set(2, 5, 2);
    scene.add(dirLight);

    const camera = new THREE.PerspectiveCamera(40, width / height, 0.25, 50);
    camera.position.set(0, 1.5, 4);
    camera.lookAt(0, 1, 0);

    scene.add(sourceScene);
    scene.add(targetScene);

    sourceScene.position.x -= 0.8;
    targetScene.position.x += 0.7;
    targetScene.position.z -= 0.1;

    // Soldier model is authored in centimetres.
    targetScene.scale.setScalar(0.01);

    sourceScene.rotation.y = Math.PI / 2;
    targetScene.rotation.y = -Math.PI / 2;

    const source = getSource(sourceScene, sourceGltf.animations[0]);
    const mixer = retargetModel(source, targetScene);

    // SkinnedMesh frustum culling uses the bind-pose bounding box, which can
    // wrongly cull retargeted meshes whose hip animates far from the origin.
    targetScene.traverse((child) => {
      if ((child as THREE.SkinnedMesh).isSkinnedMesh) {
        child.frustumCulled = false;
      }
    });
    sourceScene.traverse((child) => {
      if ((child as THREE.SkinnedMesh).isSkinnedMesh) {
        child.frustumCulled = false;
      }
    });

    // floor
    const reflection = reflector();
    reflection.target.rotateX(-Math.PI / 2);
    scene.add(reflection.target);

    const floorMaterial = new THREE.NodeMaterial();
    floorMaterial.colorNode = reflection;
    floorMaterial.opacity = 0.2;
    floorMaterial.transparent = true;

    const floor = new THREE.Mesh(
      new THREE.BoxGeometry(50, 0.001, 50),
      floorMaterial,
    );
    floor.receiveShadow = true;
    floor.position.set(0, 0, 0);
    scene.add(floor);

    const renderer = makeWebGPURenderer(context, { antialias: true });
    renderer.toneMapping = THREE.NeutralToneMapping;

    const timer = new THREE.Timer();

    renderer.setAnimationLoop(() => {
      timer.update();
      const delta = timer.getDelta();
      source.mixer.update(delta);
      mixer.update(delta);
      renderer.render(scene, camera);
      context.present();
    });

    return () => {
      renderer.setAnimationLoop(null);
    };
  }, [sourceGltf, targetGltf]);

  return (
    <View style={{ flex: 1, justifyContent: "center", alignItems: "center" }}>
      <Text>Loading assets...</Text>
      <View style={StyleSheet.absoluteFill}>
        <Canvas ref={ref} style={{ flex: 1 }} />
      </View>
    </View>
  );
};
