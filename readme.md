# Experiments!

[![Build Status](https://travis-ci.org/ddiakopoulos/sandbox.svg?branch=master)](https://travis-ci.org/ddiakopoulos/sandbox)

A collection of C++11 classes and GLSL shaders united under the arbitrarily chosen namespace "avl" (short for Anvil). 

Not all experiments are supported on both XCode and VS2015 (project files are manually maintained). GLFW is required via Homebrew on OSX, while the Windows version uses a source-code copy included directly in this repository. librealsense is an optional git submodule.

## Recent
* Lightweight, header-only opengl wrapper (GL_API.hpp)
* GLSL shader program hot-reload (via efsw library)
* HDR + bloom + tonemapping pipeline
* Arkano22 SSAO implementation
* Several GLSL 330 post-processing effects: film grain, FXAA
* 2D resolution-independent layout system for screen debug output (textures, etc)
* Simple (unoptimized) PCF shadow mapping with a variety of lights (directional, spot, point)
* Instanced rendering support
* Decal projection on arbitrary meshes
* Spherical environment mapping (matcap shading)
* Billboarded triangle mesh line renderer
* Library of procedural meshes (cube, sphere, cone, torus, capsule, etc)
* Simplex noise generator (flow, derivative, curl, worley, fractal, and more)
* 2D & 3D poisson disk distribution generator
* Opinionated gizmo tool for editing 3D scenes
* Frame capture => GIF export
* Dear Imgui Bindings
* Reaction-diffusion CPU simulation (Gray-Scott)
* Gamedev and Procedural Geometry Algorithms
  * Ballistic trajectories - firing solutions for static and dynamic targets (i.e. tower defense)
  * Parabolic pointer - the computational geometry side of a teleportation mechanic for virtual reality
  * Parallel transport frames (PTF) - useful for creating tube geometries and virtual camera trajectories
  * Supershapes - modeling framework for natural forms based on the Gielis superformula
* Environment Simulation
  * Procedural sky dome with Preetham and Hosek-Wilkie models
  * Island terrain with a simple water shader (waves, reflections)
* DSP Algorithms & Filters
    * Euclidean patterns
    * PID controller
    * One-euro, complimentary, single/double exponential
* Incubation of computer vision utilities on top of [librealsense](https://www.github.com/IntelRealSense/librealsense) (minivision)
