# Experiments!

A collection of C++11 classes and GLSL shaders united under the arbitrarily chosen namespace "avl" (short for Anvil). 

## Recent

* Game-Dev, Procedural Geometry, and Simulation Algorithms
  * Ballistic trajectories - firing solutions for static and dynamic targets (i.e. tower defense)
  * Parabolic pointer - the computational geometry impl of a teleportation mechanic for VR
  * Parallel transport frames (PTF) - useful for creating tube geometries and virtual camera trajectories
  * Supershapes - modeling framework for natural forms based on the Gielis superformula (patented, unfortunately)
  * Library of procedural meshes (cube, sphere, cone, torus, capsule, etc)
  * Simplex noise generator (flow, derivative, curl, worley, fractal, and more)
  * 2D & 3D poisson disk distribution generator
  * Reaction-diffusion CPU simulation (Gray-Scott)
  * (MP/SP)-(MC/SC) Lock-free Queues
  * Euclidean patterns (Bjorklund Algo)
* Rendering
  * Lightweight, header-only opengl wrapper (GL_API.hpp)
  * Decal projection on arbitrary meshes
  * Spherical environment mapping (matcap shading)
  * Billboarded triangle mesh line renderer
  * HDR + bloom + tonemapping pipeline sample
  * Arkano22 SSAO sample  
  * Several GLSL 330 post-processing effects: film grain, FXAA
  * Simple, non-optimal PCF shadow mapping with several light types (directional, spot, point)
* App-Dev
  * GLSL shader program hot-reload (via efsw library)
  * 2D resolution-independent layout system for screen debug output (textures, etc)
  * Frame capture => GIF export
  * Dear-Imgui Bindings
  * Procedural sky dome with Preetham and Hosek-Wilkie models
  * Island terrain with a simple water shader (waves, reflections)
* DSP Algorithms & Filters
  * PID controller
  * One-euro, complimentary, single/double exponential filters for 1D data
* Samples
  * OpenVR sandbox with Bullet physics (for testing various VR-UX ideas)
  * A half-finished educational pathtracer started during a vacation in 2016