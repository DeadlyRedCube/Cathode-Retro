![Cathode Retro Logo](https://github.com/DeadlyRedCube/Cathode-Retro/blob/main/Images/CathodeRetroLogo-CRTSmall.png)
## Table of Contents
* [Introduction](#introduction)
* [Screenshots](#screenshots)
* [Features](#features)
* [Contents and Usage](#contents-and-usage)
* [Documentation](#documentation)
* [Roadmap](#roadmap)
* [License](#license)

## Introduction

`Cathode Retro` is a collection of shaders that combine to emulate the properties and artifacts of a color NTSC TV signal as well as the visual look of a Cathode-Ray Tube (CRT) TV.

## Screenshots

<p align="center">(click screenshots for full-sized version)</p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen01-Full-MopOfDestiny.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen01-Small-MopOfDestiny.png" alt="Detail of screenshot from Mop of Destiny">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen01-Full-MopOfDestiny.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen01-Detail-MopOfDestiny.png" alt="Detail of screenshot from Mop of Destiny">
  </a>
</p>
<p align="center">Screenshot from <a href="https://mopofdestiny.com">Mop of Destiny</a></p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen02-Full-SaltsmanAmarelo.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen02-Small-SaltsmanAmarelo.png" alt="Preview of Adam Saltsman's Amarelo tile set">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen02-Full-SaltsmanAmarelo.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen02-Detail-SaltsmanAmarelo.png" alt="Detail from preview of Adam Saltsman's Amarelo tile set">
  </a>
</p>
<p align="center">Image of <a href="https://itch.io/queue/c/376872/public-domain-pixel-art?game_id=560830">Amarelo tileset</a> by Adam Saltsman</p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen03-Full-SaltsmanKyst.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen03-Small-SaltsmanKyst.png" alt="Preview of Adam Saltsman's Kyst tile set">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen03-Full-SaltsmanKyst.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen03-Detail-SaltsmanKyst.png" alt="Detail from preview of Adam Saltsman's Kyst tile set">
  </a>
</p>
<p align="center">Image of <a href="https://itch.io/queue/c/376872/public-domain-pixel-art?game_id=329674">Kyst tileset</a> by Adam Saltsman</p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen04-Full-Breakout.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen04-Small-Breakout.png" alt="Screenshot from an old, unreleased brick-breaking game">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen04-Full-Breakout.jpg">
    <img src="https://cathoderetro.com/CathodeRetro-Screen04-Detail-Breakout.png" alt="Detail of screenshot from an old, unreleased brick-breaking game">
  </a>
</p>
<p align="center">Screenshot from an old, unreleased brick-breaking game</p>

## Features
* Emulate [composite](https://en.wikipedia.org/wiki/Composite_video) and [S-Video](https://en.wikipedia.org/wiki/S-Video) [NTSC](https://en.wikipedia.org/wiki/NTSC) signals
	* Using any RGB source
	* At arbitrary resolutions (not limited to standard NTSC limitations)
	* Built-in scanline timings to emulate NES/SNES and PC Composite (320- and 640-wide) displays, but flexible enough to emulate any timings
	* Noise, picture instability, and ghosting for that "my TV has bad reception" feel
	* Tint/Saturation/Brightness/Sharpness "knobs" controls, like a TV had!
	* Has correct emulation of [NTSC composite artifact colors](https://en.wikipedia.org/wiki/Composite_artifact_colors)
* Emulate an image being displayed through a [CRT](https://en.wikipedia.org/wiki/Cathode-ray_tube) monitor
	* Flat or curved screens, with optional edge and corner rounding
	* Supports emulation of [shadow mask](https://en.wikipedia.org/wiki/Shadow_mask), [slot mask](https://en.wikipedia.org/wiki/Shadow_mask), and [aperture grille](https://en.wikipedia.org/wiki/Aperture_grille) TVs
	* With or without visible scanlines
	* Approximation of CRT diffusion (the light from the TV refracting through imperfections in the glass face)
* Best at 1080p resolution and higher (great at 4k!)

## Contents and Usage

This repository contains:
* [**Shaders**](https://github.com/DeadlyRedCube/Cathode-Retro/tree/main/Shaders): All of the shader source files
	* While the shader files' extension is `hlsl`, these shaders will compile as either HLSL or GLSL, due to some macros in `cathode-retro-util-language-helpers.hlsli`
		* Compiling the shaders as HLSL requires an `HLSL` preprocessor definition be added (either by the compiler via the command line or manually at the top of `cathode-retro-util-language-helpers.hlsli`
		* Compiling for GLSL requires a loader that handles `#include` directives, as well as requires a `#version` directive (at least `#version 330 core`). See `GLHelpers.h` in `Samples/GL-Sample` for an example of this if needed
* [**Include/CathodeRetro**](https://github.com/DeadlyRedCube/Cathode-Retro/tree/main/Include/CathodeRetro): Header-only C++ code to support a `CathodeRetro::CathodeRetro` class that handles running all of the shader stages for the full effect.
	* Code requires at least C++14, and has been tested in Visual Studio 2022, and with Clang 9, Clang 17, GCC 8.1, and GCC 13.2
	* Documentation in the  [**docs**](https://github.com/DeadlyRedCube/Cathode-Retro/tree/main/docs) directory. Documentation is also available at [https://cathoderetro.com/docs](https://cathoderetro.com/docs).
* [**Samples**](https://github.com/DeadlyRedCube/Cathode-Retro/tree/main/Samples): Some C++ samples for how to use `Cathode Retro`
	* **D3D11-Sample**: A sample Visual Studio 2022 project that runs `Cathode Retro` in Direct3D 11, as HLSL shaders
	* **GL-Sample**: A sample Visual Studio 2022 project that runs `Cathode Retro` in OpenGL 3.3 core
		* Sorry, Linux/Mac users: the demo code is rather Windows-specific at the moment, but hopefully it still gives you the gist of how to hook everything up

## Documentation

The most current documentation can be found [here](https://cathoderetro.com/docs).
Documentation specific to the version of `Cathode Retro` you have checked out can be found in the [**docs**](https://github.com/DeadlyRedCube/Cathode-Retro/tree/main/docs) directory.

## Roadmap

Some things that are on the list to do at some point:
* Add more preset NTSC timing data (for instance, get the timings for Sega Genesis games, so that emulators will get that classic rainbow-like waterfall effect in Sonic)
* Add explicit support for PAL/SECAM signal emulation as well
 	- Not entirely sure how complex this is, I haven't really looked deeply into the differences in signal, decoding, and artifacts yet.
* Add additional input types (not just RGB)
	- The NES outputs in effectively 9-bit color (6 bits of color palette space plus 3 bits for "color emphasis"), and the signal can be generated direct from that
	- CGA cards had two different modes of generating a composite signal, and to truly get accurate colors for composite artifact color tricks that old PC games like Maniac Mansion used, the signal would have to be generated in the same way
* Add ability to decode a real NTSC signal - Rather than using the Generator shaders to create NTSC scanlines, it's absolutely possible to take a true NTSC signal, slice it up into scanlines, and then run it directly into the Decoder shaders
	- This would likely require a resampler from whatever the input rate is into one where the color carrier frequency is a nice even value like 4 or 8 texels (the generator by default uses 4).
	- Additionally detecting the various different ways that devices generated their scanlines, both for standard interlaced signals and the "240p" modes that consoles tended to use.
	- This is code that I have half-working using the output of an oscilloscope that I can hook a console up to, but it's sort of hacked together at the moment.
* Integration into some existing emulators
	- It's totally possible to get these into something like [RetroArch](https://www.retroarch.com/), but it's not *quite* as easy as just dropping the shaders in, requiring a little bit of redo on some of the shaders (one in particular is intended to run once for any given output screen resolution as it would be expensive to run every frame).
	
## License

>You can check out the full license [here](https://github.com/DeadlyRedCube/cathode-retro/blob/main/LICENSE).

This project is licensed under the terms of the **MIT** license.
Attribution (credit) would be greatly appreciated. If you use this, let me know! I want to see your cool projects!
