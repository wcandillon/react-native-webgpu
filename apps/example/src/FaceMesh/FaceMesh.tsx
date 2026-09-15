import React, { useEffect, useMemo, useRef, useState } from "react";
import { StyleSheet, Text, TouchableOpacity, View } from "react-native";
import * as THREE from "three";
import * as tf from "@tensorflow/tfjs";
import * as faceLandmarks from "@tensorflow-models/face-landmarks-detection";

import { fetchAsset } from "../components/useAssets";
import { makeWebGPURenderer } from "../ThreeJS/components/makeWebGPURenderer";
import { coverScale, uprightToCanvas } from "../VisionCamera/orientation";
import { ensureTfjsWebGPU } from "../VisionCamera/tfjs";
import {
  useCameraInference,
  type UprightFrameInfo,
} from "../VisionCamera/useCameraInference";

import { FACE_MESH_INDICES, FACE_MESH_UVS } from "./faceMeshData";
import { makeGlasses, makeMustache, makeTopHat } from "./props";

// MediaPipe Face Mesh: 468 landmarks, rendered by three.js on top of the
// live camera as a textured mask and as anchored 3D props.
//
// Rendering
// ---------
// three.js owns the canvas here. The camera worklet blits each upright frame
// into an rgba8 texture that three.js samples through a THREE.ExternalTexture
// on a full-screen plane; the face mesh and props sit in front of it and are
// updated from the landmarks after every inference. World space is the
// canvas with y down, 1 unit = the canvas height, x running 0..aspect, so
// distances are the same in every direction and props keep their shape.
const INPUT_SIZE = 256;
const NUM_LANDMARKS = 468;
const CAMERA_TEX_WIDTH = 720;
const CAMERA_TEX_HEIGHT = 1280;
// Exponential smoothing for the prop transforms (1 = no smoothing).
const SMOOTHING = 0.55;

// Landmark indices (MediaPipe canonical face mesh).
const LM = {
  foreheadTop: 10,
  chin: 152,
  underNose: 2,
  upperLip: 0,
  eyeA: [263, 362, 386, 374],
  eyeB: [33, 133, 159, 145],
};

type LookId = "paint" | "mesh" | "glasses" | "hat" | "mustache";
const LOOKS: { id: LookId; label: string }[] = [
  { id: "paint", label: "Paint" },
  { id: "mesh", label: "Mesh" },
  { id: "glasses", label: "Glasses" },
  { id: "hat", label: "Hat" },
  { id: "mustache", label: "Mustache" },
];
const DEFAULT_LOOKS: Record<LookId, boolean> = {
  paint: false,
  mesh: false,
  glasses: true,
  hat: true,
  mustache: true,
};

interface PipelineState {
  cameraView: GPUTextureView;
}

interface Prop {
  group: THREE.Group;
  position: THREE.Vector3;
  quaternion: THREE.Quaternion;
  scale: number;
}

interface SceneState {
  renderer: THREE.WebGPURenderer;
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  aspect: number;
  backgroundUVs: THREE.BufferAttribute;
  backgroundPositions: THREE.BufferAttribute;
  positions: THREE.BufferAttribute;
  paintMesh: THREE.Mesh;
  wireMesh: THREE.Mesh;
  props: Record<"glasses" | "hat" | "mustache", Prop>;
  tracking: boolean;
  canvasWidth: number;
  canvasHeight: number;
  lastFrame: UprightFrameInfo;
}

const loadDetector = async () => {
  await ensureTfjsWebGPU();
  return faceLandmarks.createDetector(
    faceLandmarks.SupportedModels.MediaPipeFaceMesh,
    { runtime: "tfjs", refineLandmarks: false, maxFaces: 1 },
  );
};

// Decodes a bundled image straight into a GPUTexture on the render device.
const uploadTexture = async (device: GPUDevice, mod: number) => {
  const response = await fetchAsset(mod);
  const buffer = await response.arrayBuffer();
  const bitmap = await createImageBitmap(buffer);
  const texture = device.createTexture({
    size: [bitmap.width, bitmap.height, 1],
    format: "rgba8unorm",
    usage:
      GPUTextureUsage.TEXTURE_BINDING |
      GPUTextureUsage.COPY_DST |
      GPUTextureUsage.RENDER_ATTACHMENT,
  });
  device.queue.copyExternalImageToTexture({ source: bitmap }, { texture }, [
    bitmap.width,
    bitmap.height,
  ]);
  return { texture, width: bitmap.width, height: bitmap.height };
};

// Bridges a GPUTexture we own into three.js as a sampleable 2D texture.
const wrapTexture = (texture: GPUTexture, width: number, height: number) => {
  const wrapped = new THREE.ExternalTexture(texture);
  wrapped.colorSpace = THREE.SRGBColorSpace;
  (wrapped as unknown as { image: unknown }).image = { width, height };
  wrapped.needsUpdate = true;
  return wrapped;
};

const makeProp = (group: THREE.Group): Prop => {
  group.visible = false;
  group.traverse((child) => {
    child.frustumCulled = false;
  });
  return {
    group,
    position: new THREE.Vector3(),
    quaternion: new THREE.Quaternion(),
    scale: 0,
  };
};

// Cover-fit the camera plane to the canvas whenever the frame size changes.
const updateBackground = (s: SceneState, frame: UprightFrameInfo) => {
  if (
    frame.width === 0 ||
    (frame.width === s.lastFrame.width && frame.height === s.lastFrame.height)
  ) {
    return;
  }
  s.lastFrame = frame;
  const scale = coverScale(
    s.canvasWidth,
    s.canvasHeight,
    frame.width,
    frame.height,
  );
  for (let i = 0; i < s.backgroundPositions.count; i++) {
    // Plane vertices run -aspect/2..aspect/2 and -0.5..0.5 around the plane
    // center; with the y-down camera that is canvas UV space directly.
    const cu = s.backgroundPositions.getX(i) / s.aspect + 0.5;
    const cv = s.backgroundPositions.getY(i) + 0.5;
    s.backgroundUVs.setXY(
      i,
      0.5 + (cu - 0.5) * scale[0],
      0.5 + (cv - 0.5) * scale[1],
    );
  }
  s.backgroundUVs.needsUpdate = true;
};

const landmarkAt = (array: ArrayLike<number>, i: number) =>
  new THREE.Vector3(array[i * 3], array[i * 3 + 1], array[i * 3 + 2]);

const averageOf = (array: ArrayLike<number>, indices: number[]) => {
  const v = new THREE.Vector3();
  for (const i of indices) {
    v.add(landmarkAt(array, i));
  }
  return v.multiplyScalar(1 / indices.length);
};

const placeProp = (
  prop: Prop,
  position: THREE.Vector3,
  quaternion: THREE.Quaternion,
  scale: number,
) => {
  if (prop.scale === 0) {
    prop.position.copy(position);
    prop.quaternion.copy(quaternion);
    prop.scale = scale;
  } else {
    prop.position.lerp(position, SMOOTHING);
    prop.quaternion.slerp(quaternion, SMOOTHING);
    prop.scale += (scale - prop.scale) * SMOOTHING;
  }
  prop.group.position.copy(prop.position);
  prop.group.quaternion.copy(prop.quaternion);
  prop.group.scale.setScalar(prop.scale);
};

// Builds a face-aligned frame from the landmarks and drops every prop on
// its anchor. Local +y is toward the top of the head, +z toward the camera.
const updateProps = (s: SceneState) => {
  const array = s.positions.array as ArrayLike<number>;
  const eyeA = averageOf(array, LM.eyeA);
  const eyeB = averageOf(array, LM.eyeB);
  const foreheadTop = landmarkAt(array, LM.foreheadTop);
  const chin = landmarkAt(array, LM.chin);

  const up = foreheadTop.clone().sub(chin).normalize();
  let right = eyeA.clone().sub(eyeB).normalize();
  const forward = new THREE.Vector3().crossVectors(right, up).normalize();
  if (forward.z < 0) {
    // Keep +z pointing at the camera; the props are symmetric so which
    // side ends up +x does not matter.
    right.negate();
    forward.negate();
  }
  right = new THREE.Vector3().crossVectors(up, forward).normalize();
  const quaternion = new THREE.Quaternion().setFromRotationMatrix(
    new THREE.Matrix4().makeBasis(right, up, forward),
  );
  const eyeDist = eyeA.distanceTo(eyeB);

  const eyeMid = eyeA.clone().add(eyeB).multiplyScalar(0.5);
  placeProp(
    s.props.glasses,
    eyeMid.addScaledVector(forward, 0.18 * eyeDist),
    quaternion,
    eyeDist,
  );

  const hatAnchor = foreheadTop
    .clone()
    .addScaledVector(up, 0.25 * eyeDist)
    .addScaledVector(forward, -0.25 * eyeDist);
  placeProp(s.props.hat, hatAnchor, quaternion, eyeDist);

  const mustacheAnchor = landmarkAt(array, LM.underNose)
    .lerp(landmarkAt(array, LM.upperLip), 0.55)
    .addScaledVector(forward, 0.08 * eyeDist);
  placeProp(s.props.mustache, mustacheAnchor, quaternion, eyeDist);
};

const applyVisibility = (s: SceneState, on: Record<LookId, boolean>) => {
  const { tracking } = s;
  s.paintMesh.visible = tracking && on.paint;
  s.wireMesh.visible = tracking && on.mesh;
  s.props.glasses.group.visible = tracking && on.glasses;
  s.props.hat.group.visible = tracking && on.hat;
  s.props.mustache.group.visible = tracking && on.mustache;
};

export const FaceMesh = () => {
  const [status, setStatus] = useState("Loading face mesh model...");
  const [looks, setLooks] = useState(DEFAULT_LOOKS);
  const looksRef = useRef(looks);
  const sceneRef = useRef<SceneState | null>(null);

  const detectorPromise = useMemo(() => {
    const p = loadDetector();
    p.then(() => setStatus("Tracking face...")).catch(() => {});
    return p;
  }, []);

  const { element, error, getFrameInfo } = useCameraInference<PipelineState>({
    inputSize: INPUT_SIZE,
    cameraPosition: "front",
    setup: async ({ device, context, canvasWidth, canvasHeight }) => {
      // Upright camera copy written by the worklet, sampled by three.js.
      const cameraTexture = device.createTexture({
        size: [CAMERA_TEX_WIDTH, CAMERA_TEX_HEIGHT],
        format: "rgba8unorm",
        usage:
          GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
      });
      const paintTexture = await uploadTexture(
        device,
        require("./assets/face-paint.png"),
      );
      const wireTexture = await uploadTexture(
        device,
        require("./assets/face-uv.png"),
      );

      const renderer = makeWebGPURenderer(context, { device });
      await renderer.init();

      const aspect = canvasWidth / canvasHeight;
      const scene = new THREE.Scene();
      // Canvas space, y down, 1 unit = canvas height.
      const camera = new THREE.OrthographicCamera(0, aspect, 0, 1, -10, 10);

      const backgroundGeometry = new THREE.PlaneGeometry(aspect, 1);
      const background = new THREE.Mesh(
        backgroundGeometry,
        new THREE.MeshBasicMaterial({
          map: wrapTexture(cameraTexture, CAMERA_TEX_WIDTH, CAMERA_TEX_HEIGHT),
          side: THREE.DoubleSide,
          toneMapped: false,
        }),
      );
      background.position.set(aspect / 2, 0.5, -1);
      scene.add(background);

      // Lights for the props. World y is down, so "above" is negative y.
      scene.add(new THREE.AmbientLight(0xffffff, 0.9));
      const key = new THREE.DirectionalLight(0xffffff, 2.2);
      key.position.set(0.4, -1, 1.5);
      scene.add(key);

      const faceGeometry = new THREE.BufferGeometry();
      const positions = new THREE.BufferAttribute(
        new Float32Array(NUM_LANDMARKS * 3),
        3,
      );
      positions.setUsage(THREE.DynamicDrawUsage);
      faceGeometry.setAttribute("position", positions);
      faceGeometry.setAttribute(
        "uv",
        new THREE.BufferAttribute(new Float32Array(FACE_MESH_UVS), 2),
      );
      faceGeometry.setIndex(FACE_MESH_INDICES);

      const paintMesh = new THREE.Mesh(
        faceGeometry,
        new THREE.MeshBasicMaterial({
          map: wrapTexture(
            paintTexture.texture,
            paintTexture.width,
            paintTexture.height,
          ),
          transparent: true,
          side: THREE.DoubleSide,
          toneMapped: false,
        }),
      );
      // The MediaPipe UV chart is a gray wireframe; multiplying it onto the
      // face draws the mesh lines without tinting the skin.
      const wireMesh = new THREE.Mesh(
        faceGeometry,
        new THREE.MeshBasicMaterial({
          map: wrapTexture(
            wireTexture.texture,
            wireTexture.width,
            wireTexture.height,
          ),
          transparent: true,
          blending: THREE.MultiplyBlending,
          side: THREE.DoubleSide,
          toneMapped: false,
        }),
      );
      for (const mesh of [paintMesh, wireMesh]) {
        mesh.visible = false;
        mesh.frustumCulled = false;
        scene.add(mesh);
      }

      const props = {
        glasses: makeProp(makeGlasses()),
        hat: makeProp(makeTopHat()),
        mustache: makeProp(makeMustache()),
      };
      for (const prop of Object.values(props)) {
        scene.add(prop.group);
      }

      const state: SceneState = {
        renderer,
        scene,
        camera,
        aspect,
        backgroundUVs: backgroundGeometry.getAttribute(
          "uv",
        ) as THREE.BufferAttribute,
        backgroundPositions: backgroundGeometry.getAttribute(
          "position",
        ) as THREE.BufferAttribute,
        positions,
        paintMesh,
        wireMesh,
        props,
        tracking: false,
        canvasWidth,
        canvasHeight,
        lastFrame: { width: 0, height: 0 },
      };
      sceneRef.current = state;

      renderer.setAnimationLoop(() => {
        updateBackground(state, getFrameInfo());
        renderer.render(scene, camera);
        context.present();
      });

      return { cameraView: cameraTexture.createView() };
    },
    render: ({
      device,
      externalTexture,
      blitPipeline,
      blitUniformBuffer,
      sampler,
      pipelineState,
    }) => {
      "worklet";
      // Upright copy of the frame for three.js; the blit uniform already
      // carries this frame's rotation and mirror flags.
      const bindGroup = device.createBindGroup({
        layout: blitPipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: sampler },
          { binding: 2, resource: { buffer: blitUniformBuffer } },
        ],
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: pipelineState.cameraView,
            clearValue: { r: 0, g: 0, b: 0, a: 1 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.setPipeline(blitPipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      pass.end();
      device.queue.submit([encoder.finish()]);
    },
    inference: async (rgb, size) => {
      const detector = await detectorPromise;
      const s = sceneRef.current;
      if (!s) {
        return;
      }
      const tensor = tf.tensor3d(rgb, [size, size, 3]);
      let faces: faceLandmarks.Face[];
      try {
        faces = await detector.estimateFaces(tensor, { flipHorizontal: false });
      } finally {
        tensor.dispose();
      }
      if (faces.length === 0) {
        s.tracking = false;
        applyVisibility(s, looksRef.current);
        return;
      }
      const frame = getFrameInfo();
      if (frame.width === 0) {
        return;
      }
      // Landmarks are in model-input pixels. Map them through the same
      // cover fit as the background so they land on the face on screen.
      // The model input is the upright image stretched to a square, so one
      // input pixel spans aspect / (size * scale.x) world units in x; z is
      // reported at the same scale as x.
      const scale = coverScale(
        s.canvasWidth,
        s.canvasHeight,
        frame.width,
        frame.height,
      );
      const zScale = s.aspect / (size * scale[0]);
      const [{ keypoints }] = faces;
      const { array } = s.positions;
      const n = Math.min(NUM_LANDMARKS, keypoints.length);
      for (let i = 0; i < n; i++) {
        const [x, y] = uprightToCanvas(
          keypoints[i].x / size,
          keypoints[i].y / size,
          scale,
        );
        array[i * 3] = x * s.aspect;
        array[i * 3 + 1] = y;
        // Smaller landmark z means closer to the camera; the ortho camera
        // looks down -z, so flip the sign to keep the nose in front.
        array[i * 3 + 2] = -(keypoints[i].z ?? 0) * zScale;
      }
      s.positions.needsUpdate = true;
      updateProps(s);
      s.tracking = true;
      applyVisibility(s, looksRef.current);
    },
  });

  useEffect(
    () => () => {
      sceneRef.current?.renderer.setAnimationLoop(null);
    },
    [],
  );

  const toggleLook = (id: LookId) => {
    const next = { ...looksRef.current, [id]: !looksRef.current[id] };
    looksRef.current = next;
    setLooks(next);
    const s = sceneRef.current;
    if (s) {
      applyVisibility(s, next);
    }
  };

  return (
    <View style={styles.root}>
      {element}
      <View style={styles.statusBar}>
        <Text style={error ? styles.errorText : styles.statusText}>
          {error ?? status}
        </Text>
      </View>
      <View style={styles.toolbar}>
        {LOOKS.map((look) => (
          <TouchableOpacity
            key={look.id}
            onPress={() => toggleLook(look.id)}
            style={[styles.chip, looks[look.id] && styles.chipActive]}
          >
            <Text
              style={[styles.chipText, looks[look.id] && styles.chipTextActive]}
            >
              {look.label}
            </Text>
          </TouchableOpacity>
        ))}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "black" },
  statusBar: {
    position: "absolute",
    top: 16,
    left: 16,
    right: 16,
    backgroundColor: "rgba(0,0,0,0.55)",
    paddingHorizontal: 10,
    paddingVertical: 6,
    borderRadius: 6,
  },
  statusText: { color: "white", fontSize: 12 },
  errorText: { color: "#ff6b6b", fontSize: 12 },
  toolbar: {
    position: "absolute",
    bottom: 32,
    left: 12,
    right: 12,
    flexDirection: "row",
    flexWrap: "wrap",
    justifyContent: "center",
    gap: 8,
  },
  chip: {
    backgroundColor: "rgba(0,0,0,0.55)",
    paddingHorizontal: 14,
    paddingVertical: 8,
    borderRadius: 18,
    borderWidth: 1,
    borderColor: "rgba(255,255,255,0.35)",
  },
  chipActive: {
    backgroundColor: "rgba(255,255,255,0.9)",
    borderColor: "white",
  },
  chipText: { color: "white", fontSize: 13, fontWeight: "600" },
  chipTextActive: { color: "black" },
});
