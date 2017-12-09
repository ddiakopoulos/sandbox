# Graphics Sandbox

[![License is Unlicense](http://img.shields.io/badge/license-Unlicense-blue.svg?style=flat)](http://unlicense.org/)

Platform | Build Status |
-------- | ------------ |
Visual Studio 2017 | [![Build status](https://ci.appveyor.com/api/projects/status/t1v586iy881ptlql?svg=true)](https://ci.appveyor.com/project/ddiakopoulos/sandbox)

A collection of C++11/14 classes and opengl utilities united under the arbitrarily namespace "avl" (short for Anvil). 

<p align="center">
  <img src="https://raw.githubusercontent.com/ddiakopoulos/sandbox/master/assets/images/sandbox-cover.png"/>
</p>

## Project Structure

  * `scene-editor\` - A scene editor
  * `particle-system\` - Work-in-progress CPU and GPU particle system simulator and renderer
  * `projective-texturing\` - Implements projective surface texturing (i.e. Unity's projector object)
  * `portal-rendering\` - A simple sample for source/target portal rendering. Does not handle nested portals.
  * `vr-environment\` - Incomplete sample for physics + VR interaction using Bullet 3
  * `clustered-shading\` - A minimally viable implementation of clustered shading for point-lights
  * `lib-render\` -  Implements a forward renderer, physically-based materials, and asset serialization. 
  * `lib-incubator\` - Shared code between public & private projects.
  * `examples\` - Tests of functionality in lib-incubator. Not likely to compile if you are not me. 

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
  * Kmeans clustering of pointclouds
  * Quickhull of pointclouds
* Rendering
  * Lightweight, header-only opengl wrapper (`gl-api.hpp`)
  * Decal projection on arbitrary meshes
  * Spherical environment mapping (matcap shading)
  * Billboarded triangle mesh line renderer
  * Procedural sky dome with Preetham and Hosek-Wilkie models
  * Island terrain with a simple water shader (waves, reflections)
* App-Dev
  * GLSL shader program hot-reload (via efsw library)
  * 2D resolution-independent layout system for screen debug output (textures, etc)
  * Dear-Imgui Bindings
* DSP Algorithms & Filters
  * One-euro, complimentary, and exponential filters for 1D data
  * Ad-hoc statistical analysis (variance, skewness, kurtosis, etc)

`lib-incubator` incorporates a considerable amount of permissively-licensed and public-domain code adapted for `linalg.h` types or readability/usability/extensibility (like various lock-free queues and graphics samples). See `COPYING` for full attribution of code found in this repository. 