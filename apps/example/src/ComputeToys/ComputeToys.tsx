/* eslint-disable max-len */
import { useCallback, useEffect, useRef, useState } from "react";
import { Canvas, useCanvasEffect } from "react-native-wgpu";
import { useWindowDimensions } from "react-native";

import { ComputeEngine } from "./engine";

export const ComputeToys = () => {
  const { width, height } = useWindowDimensions();
  const [engine, setEngine] = useState<ComputeEngine | null>(null);
  const animationRef = useRef<number | null>(null);
  const lastTimeRef = useRef<number>(0);

  // Add your shader code here
  const shaderCode = /* wgsl */ `// MIT License. © 2023 munrocket

  const EPS = 0.005;
  const FAR = 10.0;
  const AA = 2.;
  const PI = 3.1415926;
  
  fn rotX(p: vec3f, a: f32) -> vec3f { let r = p.yz * cos(a) + vec2f(-p.z, p.y) * sin(a); return vec3f(p.x, r); }
  fn rotY(p: vec3f, a: f32) -> vec3f { let r = p.xz * cos(a) + vec2f(-p.z, p.x) * sin(a); return vec3f(r.x, p.y, r.y); }
  fn rotM(p: vec3f, m: vec2f) -> vec3f { return rotY(rotX(p, -PI * m.y), 2 * PI * m.x); }
  
  fn opSmoothUnion(d1: f32, d2: f32, k: f32) -> f32 {
    let h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0., 1.);
    return mix(d2, d1, h) - k * h * (1. - h);
  }
  
  fn opSmoothSubtract(d1: f32, d2: f32, k: f32) -> f32 {
    let h = clamp(0.5 - 0.5 * (d1 + d2) / k, 0., 1.);
    return mix(d1, -d2, h) + k * h * (1. - h);
  }
  
  fn map(pos: vec3f) -> f32 {
      let k = 0.1;
      let d = length(vec3(abs(pos.x), pos.yz) - vec3(0.55, -0.15, 0.)) - 0.48;
      let d2 = opSmoothUnion(d, length(pos - vec3(0., -0.18, 0.)) - 0.45, k);
      let d3 = opSmoothUnion(d2, length(pos - vec3(-0.28, 0.18, 0.)) - 0.4, k);
      let d4 = opSmoothUnion(d3, length(pos - vec3(0.25, 0.35, 0.)) - 0.4, k);
      return  opSmoothSubtract(d4, max(0.05+pos.x, -abs(.04*pos.y*pos.y+0.0175) + abs(.04 - 0.1*fract(0.16+pos.x/0.16))), 0.01);
  }
  
  fn normal(p: vec3f) -> vec3f {
      let e = vec2f(0., EPS);
      return normalize(vec3f(
          map(p + e.yxx) - map(p - e.yxx),
          map(p + e.xyx) - map(p - e.xyx),
          map(p + e.xxy) - map(p - e.xxy)
      ));
  }
  
  fn getBackground(uv: vec2f) -> vec3f {
      return vec3(119., 159., 191.)/250. + 0.3 * exp(0.5 - length(uv)/2.) ;
  }
  
  fn getDiffuse(v: vec3f) -> vec3f {
      var p = v;
      p.x -= .2;
      let a = atan2(p.y, p.x) + 5.;
      let r = length(p.xy);
      let s = r / (r * r + .3);
      return sqrt(sin(vec3(a,a+1.,a+3.)) * .5 * s +.5);
  }
  
  
  fn march(ro: vec3f, rd: vec3f, uv: vec2f) -> vec3f {
      var I: f32; var t = FAR; var dt: f32; var t0: f32;
      var mapSign = 1.; var col0: vec3f; var col = getBackground(uv);
      var p = ro + t * rd;
      for(var i = 0; i < 220; i++) {
          dt = map(p);
          t -= max(EPS, abs(dt));
          p = ro + t * rd;
          if (sign(dt) != mapSign) {
              if (mapSign < 0.) {
                  let si = t - t0;
                  let off = select(0., 1.8 * (1. - exp(si*1.9)), mapSign < 0.);
                  col0 = mix(col0, getDiffuse(p), .8);
                  col -= (1.-col0) * off;
              }
              I += 1.;
              mapSign = sign(dt);
              t0 = t;
              col0 = getDiffuse(p);
          }
          if (t < 0.) { break; }
      }
      if (I > 0.) {
          p = ro + t0 * rd;
          let N = normal(p);
          let r0 = 0.15;
          let schlick = r0 + (1.0 - r0) * pow(1. + dot(rd, N), 4.);
          col = mix(col, getBackground(uv), schlick);
      }
      return col;
  }
  
  @compute @workgroup_size(16, 16)
  fn main_image(@builtin(global_invocation_id) id: vec3u) {
      let res = textureDimensions(screen);
      var ro: vec3f; var rd: vec3f; var uv: vec2f; var col: vec3f;
  
      for (var i = 0.; i < AA; i += 1.) {
      for (var j = 0.; j < AA; j += 1.) {
          let dxy = (vec2f(i, j) + .5) / AA;
          uv = (2.*(vec2f(id.xy) + dxy) - vec2f(res)) / f32(res.y);
          ro = vec3f(0, 0, 5.);
          rd = normalize(vec3f(uv, -2));
          let mousepos = vec2f(mouse.pos) / vec2f(res) - .5;
          ro = rotM(ro, mousepos);
          rd = rotM(rd, mousepos);
          col += march(ro, rd, uv) / AA / AA;
      }}
      
      if (id.x < res.x && id.y < res.y) { 
          textureStore(screen, vec2u(id.x, res.y-1-id.y), vec4f(col*col, 1.));
      }
  }`;

  // Initialize WebGPU and the compute engine
  const canvasRef = useCanvasEffect(() => {
    const initWebGPU = async () => {
      try {
        // Create the compute engine
        await ComputeEngine.create();
        const eng = ComputeEngine.getInstance();

        // Set the canvas surface
        if (canvasRef.current) {
          eng.setSurface(canvasRef.current!);

          // Set the canvas size based on your CANVAS constants
          eng.resize(width, height, window.devicePixelRatio || 1);

          // Initialize render state
          eng.reset();

          // Set callbacks for shader compilation
          eng.onSuccess((entryPoints) => {
            console.log(
              "Shader compiled successfully with entry points:",
              entryPoints,
            );
          });

          eng.onError((message, row, col) => {
            console.error(`Shader error at ${row}:${col} - ${message}`);
          });

          // Process and compile the shader
          const preprocessed = await eng.preprocess(shaderCode);
          if (preprocessed) {
            await eng.compile(preprocessed);
          }

          setEngine(eng);
        }
      } catch (error) {
        console.error("Failed to initialize WebGPU:", error);
      }
    };

    initWebGPU();

    return () => {
      if (animationRef.current !== null) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  });

  // Animation/render loop
  const renderLoop = useCallback(
    (timestamp: number) => {
      if (!engine) {
        return;
      }

      // Calculate time delta
      const delta = lastTimeRef.current
        ? (timestamp - lastTimeRef.current) / 1000
        : 0;
      lastTimeRef.current = timestamp;

      // Update time uniforms
      engine.setTimeElapsed(timestamp / 1000);
      engine.setTimeDelta(delta);

      // Render frame
      engine.render();
      // Schedule next frame
      animationRef.current = requestAnimationFrame(renderLoop);
    },
    [engine],
  );

  useEffect(() => {
    renderLoop(new Date().getTime());
  }, [renderLoop]);

  return <Canvas ref={canvasRef} style={{ width, height }} />;
};
