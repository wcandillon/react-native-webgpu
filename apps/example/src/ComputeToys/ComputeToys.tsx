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
  const shaderCode = /* wgsl */ `#define SCREEN_GRID_X 128
  #define SCREEN_GRID_Y 64
  #define GRID_STORAGE 3000
  #define PARTICLE_RAD 32.0
  
  #storage grid ScreenIndexGrid
  
  //store the id's of the particles in each screen "cell"
  struct ScreenIndexGrid 
  {
      count: array<array<atomic<u32>, SCREEN_GRID_Y>, SCREEN_GRID_X>,
      ids: array<array<array<u32, GRID_STORAGE>, SCREEN_GRID_Y>, SCREEN_GRID_X>
  }
  
  fn GetCellID(pos: uint2) -> uint2
  {
      let uv = float2(pos) / float2(SCREEN_WIDTH, SCREEN_HEIGHT);
      return uint2(floor(uv * float2(SCREEN_GRID_X, SCREEN_GRID_Y))-1);
  }
  
  fn AddToCell(cell: uint2, id: uint)
  {
      let cid = atomicAdd(&grid.count[cell.x][cell.y], 1);
      if(cid < GRID_STORAGE)
      {
          grid.ids[cell.x][cell.y][cid] = id;
      }
  }
  
  fn AddParticle(pos: uint2, id: uint)
  {
      let cell = GetCellID(pos);
      AddToCell(cell, id);
  }
  
  fn AddParticleQuad(pos0: uint2, pos1: uint2, id: uint)
  {
      let cell0 = GetCellID(pos0);
      let cell1 = GetCellID(pos1);
      
      for(var i = cell0.x; i <= cell1.x; i++)
      {
          for(var j = cell0.y; j <= cell1.y; j++)
          {
              AddToCell(uint2(i, j), id);
          }
      }
  }
  
  fn ClearCell(cell: uint2)
  {
      atomicStore(&grid.count[cell.x][cell.y], 0);
  }
   
  const MaxSamples = 8.0;
  const FOV = 0.8;
  const PI = 3.14159265;
  const TWO_PI = 6.28318530718;
  //sqrt of particle count
  const PARTICLE_COUNT = 128;
  //PARTICLE_COUNT / 16
  #define PARTICLE_COUNT_16 16
  const DEPTH_MIN = 0.2;
  const DEPTH_MAX = 5.0;
  const DEPTH_BITS = 16u;
  const dq = float2(0.0, 1.0);
  const eps = 0.01;
  
  fn GetParticleID(pix: int2) -> uint
  {
      return uint(pix.x) + uint(pix.y)*PARTICLE_COUNT;
  }
  
  fn GetParticlePix(id: uint) -> int2
  {
      return int2(int(id%PARTICLE_COUNT), int(id/PARTICLE_COUNT));
  }
  
  struct Camera 
  {
    pos: float3,
    cam: float3x3,
    fov: float,
    size: float2
  }
  
  const KerrM = 1.0;
  
  struct GeodesicRay
  {    
      q:  float4,
      qt: float4,
      p:  float4,
  }; 
  
  struct Particle
  {
      position: float4,
      velocity: float4,
  }
  
  var<private> camera : Camera;
  var<private> state : uint4;
  var<private> bokehRad : float;
  
  fn sdLine(p: float2, a: float2, b: float2) -> float
  {
      let pa = p - a;
      let ba = b - a;
      let h = clamp(dot(pa,ba)/dot(ba,ba), 0.0, 1.0);
      return length(pa - ba*h);
  }
  
  fn sqr(x: float) -> float
  {
      return x*x;
  }
  
  fn diag(a: float4) -> float4x4
  {
      return float4x4(
          a.x,0.0,0.0,0.0,
          0.0,a.y,0.0,0.0,
          0.0,0.0,a.z,0.0,
          0.0,0.0,0.0,a.w
      );
  }
  
  fn pcg4d(a: uint4) -> uint4
  {
    var v = a * 1664525u + 1013904223u;
      v.x += v.y*v.w; v.y += v.z*v.x; v.z += v.x*v.y; v.w += v.y*v.z;
      v = v ^  ( v >> uint4(16u) );
      v.x += v.y*v.w; v.y += v.z*v.x; v.z += v.x*v.y; v.w += v.y*v.z;
      return v;
  }
  
  fn rand4() -> float4
  { 
      state = pcg4d(state);
      return float4(state)/float(0xffffffffu); 
  }
  
  fn rand4s(seed: uint4) -> float4
  { 
      return float4(pcg4d(seed))/float(0xffffffffu); 
  }
  
  fn nrand4(sigma: float, mean: float4) -> float4
  {
      let Z = rand4();
      return mean + sigma * sqrt(-2.0 * log(Z.xxyy)) * 
             float4(cos(TWO_PI * Z.z),sin(TWO_PI * Z.z),cos(TWO_PI * Z.w),sin(TWO_PI * Z.w));
  }
  
  fn disk(r: float2) -> float2
  {
      return vec2(sin(TWO_PI*r.x), cos(TWO_PI*r.x))*(r.y);
  }
  
  fn GetCameraMatrix(ang0: float2) -> float3x3
  {
      let ang = ang0 + float2(0.0, 1.8);
      let x_dir = float3(cos(ang.x)*sin(ang.y), cos(ang.y), sin(ang.x)*sin(ang.y));
      let y_dir = normalize(cross(x_dir, float3(0.0,1.0,0.0)));
      let z_dir = normalize(cross(y_dir, x_dir));
      return float3x3(-x_dir, y_dir, z_dir);
  }
  
  fn SetCamera(ang: float2, fov: float)
  {
      camera.fov = fov;
      camera.cam = GetCameraMatrix(ang); 
      camera.pos = - (camera.cam*float3(50.0*custom.Radius+0.5,0.0,0.0));
      camera.size = float2(textureDimensions(screen));
  }
  
  //project to clip space
  fn Project(cam: Camera, p: float3) -> float3
  {
      let td = distance(cam.pos, p);
      let dir = (p - cam.pos)/td;
      let screen = dir*cam.cam;
      return float3(screen.yz*cam.size.y/(cam.fov*screen.x) + 0.5*cam.size,screen.x*td);
  }
  
  fn LoadParticle(pix: int2) -> Particle
  {
      var p: Particle;
      p.position = textureLoad(pass_in, pix, 0, 0); 
      p.velocity = textureLoad(pass_in, pix, 1, 0);
      return p;
  }
  
  fn SaveParticle(pix: int2, p: Particle) 
  {
      textureStore(pass_out, pix, 0, p.position); 
      textureStore(pass_out, pix, 1, p.velocity); 
  }
  
  fn KerrGetR2(p: float3) -> float
  {
      let rho = dot(p,p) - sqr(custom.KerrA);
      let r2 = 0.5*(rho + sqrt(sqr(rho) + sqr(2.0*custom.KerrA*p.z)));
      return r2;
  }
  
  fn KerrGetK(p: float3) -> float4
  {
      let r2 = KerrGetR2(p);
      let r = sqrt(r2);
      let invr2 = 1.0 / (r2 + sqr(custom.KerrA) + 1e-3); 
      let  k = float3((r*p.x - custom.KerrA*p.y) * invr2, (r*p.y + custom.KerrA*p.x) * invr2, p.z/(r + 1e-4));
      let f = r2 * (2.0 * KerrM * r - sqr(custom.KerrQ)) / (r2 * r2 + sqr(custom.KerrA * p.z) + 1e-3);
      return float4(k, f);
  }
  
  fn G(q: float4) -> float4x4 
  {
      //Kerr metric in Kerr-Schild coordinates 
      let k = KerrGetK(q.yzw);
      let kf = k.w*float4(1.0, k.xyz);
      return diag(float4(-1.0,1.0,1.0,1.0)) + float4x4(kf, k.x*kf, k.y*kf, k.z*kf);    
  }
  
  fn Ginv(q: float4) -> float4x4 
  {
      //inverse of Kerr metric in Kerr-Schild coordinates 
      let k = KerrGetK(q.yzw);
      let kf = k.w*vec4(1.0, -k.xyz)/dot(k.xyz, k.xyz);
      return diag(float4(-1.0,1.0,1.0,1.0)) + float4x4(-kf, k.x*kf, k.y*kf, k.z*kf); 
  }
  
  //lagrangian
  fn Lmat(qt: float4, g: float4x4) -> float 
  {
      return   g[0][0]*qt.x*qt.x + g[1][1]*qt.y*qt.y + g[2][2]*qt.z*qt.z + g[3][3]*qt.w*qt.w +
          2.0*(g[0][1]*qt.x*qt.y + g[0][2]*qt.x*qt.z + g[0][3]*qt.x*qt.w +
                  g[1][2]*qt.y*qt.z + g[1][3]*qt.y*qt.w +
                  g[2][3]*qt.z*qt.w);
  }
  
  fn L(qt: float4, q: float4) -> float 
  {
      return Lmat(qt, G(q));
  }
  
  fn H(p: float4, ginv: float4x4) -> float 
  {
      return Lmat(p, ginv);
  }
  
  fn  ToMomentum(ray: GeodesicRay) -> float4 
  {
      return G(ray.q)*ray.qt; 
  }
  
  fn  FromMomentum(ray: GeodesicRay) -> float4 
  {
      return Ginv(ray.q)*ray.p; 
  }
  
  fn ParticleToGeodesic(particle: Particle) -> GeodesicRay
  {
      var ray: GeodesicRay;
      ray.q = particle.position;
      ray.p = particle.velocity;
      return ray;
  }
  
  fn GeodesicToParticle(ray: GeodesicRay) -> Particle
  {
      var particle: Particle;
      particle.position = ray.q;
      particle.velocity = ray.p/length(ray.qt);
      return particle;
  }
  
  fn HamiltonianGradient(ray: GeodesicRay) -> float4 
  {
      let ginv = Ginv(ray.q);
      let H0 = H(ray.p, ginv);
      let delta = 0.1; 
      return (float4(
          L(ray.qt,ray.q+delta*dq.yxxx),
          L(ray.qt,ray.q+delta*dq.xyxx),
          L(ray.qt,ray.q+delta*dq.xxyx),
          L(ray.qt,ray.q+delta*dq.xxxy)) - H0)/delta;
  }
  
  fn VelClamp(vel: float4) -> float4
  {
      return vel;//float4(vel.x, vel.yzw / max(1.0, length(vel.yzw)));
  }
  
  @compute @workgroup_size(16, 16)
  fn SimulateParticles(@builtin(global_invocation_id) id: uint3) 
  {
      var pix = int2(id.xy);
      var p = LoadParticle(pix);
  
      if(pix.x > PARTICLE_COUNT || pix.y > PARTICLE_COUNT) 
      {   
          return;
      }
      
      state = uint4(id.x, id.y, id.z, time.frame);
      
      let r = sqrt(KerrGetR2(p.position.yzw));
  
      if(time.frame == 0u || r < 0.9 || r > 30.0)
      {
          let rng = rand4();
          let rng1 = rand4();
          p.position = 30.0*float4(1.0, 1.0, 1.0, custom.InitThick) * float4(0.0,2.0*rng.xyz - 1.0);
  
          let r01 = sqrt(KerrGetR2(p.position.yzw)); 
          if(r01 < 0.9)
          {
              return;
          }
  
          var vel = normalize(cross(p.position.yzw, float3(0.0,0.0,1.0)));
  
          vel += 0.3*(rng1.xyz * 0.5 - 0.25);
          let vscale = clamp(1.0 / (0.2 + 0.08*r01), 0., 1.0);
          p.velocity = float4(-1.0,2.0*(custom.InitSpeed - 0.5)*vel*vscale);
      }
  
     
      
      var ray = ParticleToGeodesic(p);
  
      if(mouse.click == 1) 
      {
         // return;
      }
     
      for(var i = 0; i < int(custom.Steps*16.0 + 1.0); i++)
      {
          ray.qt = FromMomentum(ray);
          let qt0 = ray.qt;
          let dt = 0.5 * custom.TimeStep / (abs(ray.qt.x) + 0.01);
          ray.p += HamiltonianGradient(ray)*dt;
          ray.qt = FromMomentum(ray);
          ray.q += (ray.qt+qt0)*dt;
      }
  
      SaveParticle(pix, GeodesicToParticle(ray));
  }
  
  #workgroup_count ClearScreenGrid 32 16 1
  @compute @workgroup_size(16, 16)
  fn ClearScreenGrid(@builtin(global_invocation_id) id: uint3) 
  {
      let cell = id.xy;
      ClearCell(cell);
  }
  
  #workgroup_count ClearGrid PARTICLE_COUNT_16 PARTICLE_COUNT_16 1
  @compute @workgroup_size(16, 16)
  fn UpdateScreenGrid(@builtin(global_invocation_id) id: uint3) {
      let screen_size = int2(textureDimensions(screen));
      let screen_size_f = float2(screen_size);
      
      let ang = float2(mouse.pos.xy)*float2(-TWO_PI, PI)/screen_size_f + 1e-4;
      
      SetCamera(ang, FOV);
  
      var pix = int2(id.xy);
  
      if(pix.x > PARTICLE_COUNT || pix.y > PARTICLE_COUNT) 
      {
          return;
      }
  
      var p = LoadParticle(pix);
      var pos = p.position.ywz;
      let projectedPos = Project(camera, pos);
      let screenCoord = int2(projectedPos.xy);
      
      //outside of our view
      if(screenCoord.x < 0 || screenCoord.x >= screen_size.x || 
          screenCoord.y < 0 || screenCoord.y >= screen_size.y || projectedPos.z < 0.0)
      {
          return;
      }
      let pos0 = uint2(clamp(screenCoord - int(PARTICLE_RAD), int2(0), screen_size));
      let pos1 = uint2(clamp(screenCoord + int(PARTICLE_RAD), int2(0), screen_size));
      AddParticleQuad(pos0, pos1, GetParticleID(pix));
  }
  
  fn hue(v: float) -> float4 {
      return .6 + .6 * cos(6.3 * v + float4(0.,23.,21.,0.));
  }
  
  fn RenderParticles(pix: uint2) -> float3
  {
      //setup camera
      let screen_size = int2(textureDimensions(screen));
      let screen_size_f = float2(screen_size);
      let ang = float2(mouse.pos.xy)*float2(-TWO_PI, PI)/screen_size_f + 1e-4;
      SetCamera(ang, FOV);
  
      //loop over particles in screen cell
      let fpix = float2(pix);
      let cell = GetCellID(pix);
      let pcount = min(atomicLoad(&grid.count[cell.x][cell.y]), GRID_STORAGE);
  
      //heatmap
      //return float3(uint3(pcount))/float(GRID_STORAGE);
  
      var color = float3(0.0);
      for(var i = 0u; i < pcount; i++)
      {
          let pid = grid.ids[cell.x][cell.y][i];
          var p = LoadParticle(GetParticlePix(pid));
          var pos = p.position.ywz;
          let vel = p.velocity.ywz;
          var ang = atan2(vel.x, vel.z+0.000001)/6.28;
          ang += (rand4s(uint4(pid, 0, 0, 0)).x - 0.5)*0.33;
          var col = hue(ang).xyz;
          
          let projectedPos0 = Project(camera, pos - vel*custom.MotionBlur);
          let projectedPos1 = Project(camera, pos + vel*custom.MotionBlur);
          let vlen = distance(projectedPos0.xy, projectedPos1.xy);
          let pdist = sdLine(fpix, projectedPos0.xy, projectedPos1.xy);
          let R = clamp(2.0*custom.BlurRadius*abs(projectedPos0.z- 100.*custom.FocalPlane), 1.5, PARTICLE_RAD);
          //color = color + 0.05*float3(1,1,1)*smoothstep(PARTICLE_RAD, 0.0, pdist)/(0.25 + pdist*pdist);
          let area = R*vlen + R*R;
          color += 20.0*col*smoothstep(R, R-1.0, pdist) / area;
      }
      let exposure = screen_size_f.x * screen_size_f.y *custom.Exposure / (896*504);
      color = 1.0 - exp(-exposure*color);
      return pow(color, float3(3.0*custom.Gamma));
  }
  
  @compute @workgroup_size(16, 16)
  fn main_image(@builtin(global_invocation_id) id: uint3) 
  {
      let screen_size = uint2(textureDimensions(screen));
  
      // Prevent overdraw for workgroups on the edge of the viewport
      if (id.x >= screen_size.x || id.y >= screen_size.y) { return; }
  
      // Pixel coordinates (centre of pixel, origin at bottom left)
      let fragCoord = float2(float(id.x) + .5, float(id.y) + .5);
  
      let color = float4(RenderParticles(id.xy), 1.0);
  
      textureStore(screen, int2(id.xy), float4(color.xyz/color.w, 1.));
  }
  `;

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
          // TODO: use PixelRatio.get()
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
          eng.setCustomFloats(
            [
              "Radius",
              "TimeStep",
              "Samples",
              "AnimatedNoise",
              "Accumulation",
              "Exposure",
              "BlurExponent1",
              "BlurRadius",
              "BlurExponent2",
              "KerrA",
              "KerrQ",
              "InitSpeed",
              "InitThick",
              "Steps",
              "FocalPlane",
              "MotionBlur",
              "Gamma",
            ],
            Float32Array.of(
              1,
              0.07199999690055847,
              0.21799999475479126,
              0,
              1,
              0.36899998784065247,
              0.3930000066757202,
              0.7429999709129333,
              0.8100000023841858,
              0.8759999871253967,
              0,
              0.718999981880188,
              0.2199999988079071,
              0.3869999945163727,
              0.5299999713897705,
              0,
              0.8270000219345093,
              0.3840000033378601,
              0.007000000216066837,
            ),
          );
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
