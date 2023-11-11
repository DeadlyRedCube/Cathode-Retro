![Cathode Retro Logo](https://cathoderetro.com/CathodeRetroLogo-CRTSmall.jpg)
## Table of Contents
* [Introduction](#introduction)
* [Screenshots](#screenshots)
* [Features](#features)
* [Contents and Usage](#contents-and-usage)
* [Using the C++ Code](#using-the-c-code)
	* [Implementing the Required Interfaces](#implementing-the-required-interfaces)
	* [Using the Main CathodeRetro class](#using-the-main-cathoderetro-class) 
* [Licence](#license)

## Introduction

`Cathode Retro` is a collection of shaders that combine to emulate the properties and artifacts of a color NTSC TV signal as well as the visual look of a Cathode-Ray Tube (CRT) TV.

## Screenshots

<p align="center">(click screenshots for full-sized version)</p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen01-Full-MopOfDestiny.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen01-Small-MopOfDestiny.png" alt="Detail of screenshot from Mop of Destiny">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen01-Full-MopOfDestiny.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen01-Detail-MopOfDestiny.png" alt="Detail of screenshot from Mop of Destiny">
  </a>
</p>
<p align="center">Screenshot from <a href="https://mopofdestiny.com">Mop of Destiny</a></p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen02-Full-SaltsmanAmarelo.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen02-Small-SaltsmanAmarelo.png" alt="Preview of Adam Saltsman's Amarelo tile set">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen02-Full-SaltsmanAmarelo.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen02-Detail-SaltsmanAmarelo.png" alt="Detail from preview of Adam Saltsman's Amarelo tile set">
  </a>
</p>
<p align="center">Image of <a href="https://itch.io/queue/c/376872/public-domain-pixel-art?game_id=560830">Amarelo tileset</a> by Adam Saltsman</p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen03-Full-SaltsmanKyst.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen03-Small-SaltsmanKyst.png" alt="Preview of Adam Saltsman's Kyst tile set">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen03-Full-SaltsmanKyst.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen03-Detail-SaltsmanKyst.png" alt="Detail from preview of Adam Saltsman's Kyst tile set">
  </a>
</p>
<p align="center">Image of <a href="https://itch.io/queue/c/376872/public-domain-pixel-art?game_id=329674">Kyst tileset</a> by Adam Saltsman</p>
<p align="center">
  <a href="https://cathoderetro.com/CathodeRetro-Screen04-Full-Breakout.png">
    <img src="https://cathoderetro.com/CathodeRetro-Screen04-Small-Breakout.png" alt="Screenshot from an old, unreleased brick-breaking game">
  </a>
  <a href="https://cathoderetro.com/CathodeRetro-Screen04-Full-Breakout.png">
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
* **Shaders**: All of the shader source files
	* While the shader files' extension is `hlsl`, these shaders will compile as either HLSL or GLSL, due to some macros in `cathode-retro-util-language-helpers.hlsli`
		* Compiling the shaders as HLSL requires an `HLSL` preprocessor definition be added (either by the compiler via the command line or manually at the top of `cathode-retro-util-language-helpers.hlsli`
		* Compiling for GLSL requires a loader that handles `#include` directives, as well as requires a `#version` directive (at least `#version 330 core`). See `GLHelpers.h` in `Samples/GL-Sample` for an example of this if needed
* **Include/CathodeRetro**: Header-only C++ code to support a `CathodeRetro::CathodeRetro` class that handles running all of the shader stages for the full effect.
	* Code requires at least C++14, and has been tested in Visual Studio 2022, and with Clang 9, Clang 17, GCC 8.1, and GCC 13.2
	* More instructions on how to use the C++ code in the [next section](#using-the-c-code)
* **Samples**: Some C++ samples for how to use `Cathode Retro`
	* **D3D11-Sample**: A sample Visual Studio 2022 project that runs `Cathode Retro` in Direct3D 11, as HLSL shaders
	* **GL-Sample**: A sample Visual Studio 2022 project that runs `Cathode Retro` in OpenGL 3.3 core
		* Sorry, Linux/Mac users: the demo code is rather Windows-specific at the moment, but hopefully it still gives you the gist of how to hook everything up

## Using the C++ Code

(More-detailed documentation of methods and parameters coming soon - for now you'll have to refer to the code)

To use the C++ code contained within `Include/CathodeRetro`:

First, ensure the `Your-Cathode-Retro-Repository-Path/Include` directory is in your project's include path.

The main class is `CathodeRetro::CathodeRetro` and is in `Include/CathodeRetro/CathodeRetro.h`. It, however, requires you to implement a set of classes derived from the interfaces in `Include/CathodeRetro/GraphicsDevice.h`, which is the interface that the class will use to load shaders, create render targets, and otherwise hook into your renderer.


### Implementing the required interfaces
To implement these classes, you'll need to
``#include "CathodeRetro/GraphicsDevice.h"``

Then you will need to implement classes derived from the interfaces in that file:
* **CathodeRetro::IGraphicsDevice**: This is the main interface that Cathode Retro uses to interact with the graphics device. It can create objects (render targets, constant buffers, shaders) and render. You'll need to implement the following methods:
	* **CreateRenderTarget**: Create a `CathodeRetro::IRenderTarget`-derived object representing a render target (or frame buffer object) with the given properties.
	*  **CreateConstantBuffer**: Create a `CathodeRetro::IConstantBuffer`-derived object that represents a block of bytes used as a constant buffer (or uniform buffer) to pass data to the shaders.
	* **CreateShader**: Create a `CathodeRetro::IShader`-derived object that represents the specified shader (requested via an ID) and whatever other associated pipeline objects are necessary to use it.
	* **BeginRendering**: This is called by the `CathodeRetro::CathodeRetro` class when it is beginning its rendering, and is where you should set up any render state that is going to be consistent across the whole pipeline (the vertex shader, blending mode, etc).
		* Cathode Retro specifically wants no alpha blending or testing enabled. 
		* Additionally, it expects floating-point textures to be able to use the full range of values, so if the API allows for truncating floating-point values to the 0..1 range on either shader output or sampling input, that should be disabled.
	* **RenderQuad**: This is called during rendering to render a full-target quad using the given `IShader`, to the given `IRenderTarget`, using a set of input `ITexture`s and an `IConstantBuffer`.
	* **EndRendering**: This is called when the `CathodeRetro::CathodeRetro` class is done rendering, and is where you should restore any render states necessary for the rest of your renderer to continue as normal.
	
* **CathodeRetro::IConstantBuffer**: This is a "constant buffer" (GL/Vulkan refer to these as "uniform buffers" - basically a data buffer to be handed to a shader. These will be fully updated every frame so it's valid for this to allocate GPU bytes out of a pool and update for graphics APIs that prefer that style of CPU -> GPU buffering. These may be updated by the `CathodeRetro::CathodeRetro` class more than once per frame. It contains the following method:
	* **Update**: Copy the given data bytes into the constant buffer so that it is ready for rendering.

* **CathodeRetro::IShader**: This is a wrapper around any shader-related objects for a specified shader. It contains no methods.
	* There is a `ShaderID` enum in this file that is used to create the shaders in the `IGraphicsDevice::CreateShader` method, and each one corresponds to a specific shader in the `Shaders/` directory.

* **CathodeRetro::IRenderTarget**: This is a wrapper around a render target (or frame buffer object), and is derived from the `CathodeRetro::ITexture` interface which contains the following methods:
	* **Width**: Get the width of this texture.
	* **Height**: Get the height of this texture.
	* **MipCount**: Get the number of mipmap levels this texture has.
	* **Format**: Returns the `CathodeRetro::TextureFormat` enum value that corresponds to the format of this texture.

You may additionally need to implement a non-render-target texture class that derives from `CathodeRetro::ITexture` to pass non-render-target textures (like, say, a CPU-updated texture for an emulator) into the `CathodeRetro::CathodeRetro::Render` method.

See the sample projects in the `Samples/` directory for reference implementations of these classes for Direct3D 11 and OpenGL 3.3 Core


### Using the Main CathodeRetro Class

Once you have the interface implemented, you can create an instance of your `CathodeRetro::IGraphicsDevice`-derived class and pass it to the `CathodeRetro::CathodeRetro` constructor to get started.

The main class requires you to
`#include "CathodeRetro/CathodeRetro.h"`

The `CathodeRetro::CathodeRetro` class contains the following public methods for you to use:
* **(constructor)**: Creates a new instance of this class, using the supplied `CathodeRetro::IGraphicsDevice` interface. Starts with an initial set of source settings.
* **UpdateSourceSettings**: Call this if the source settings change (this includes the input resolution, the signal type (RGB, composite, or S-Video), as well as any specified NTSC timings.
	* Calling this function potentially requires the recreation/reallocation of textures as settings change - these settings are not intended to change very frequently.
* **UpdateSettings**: Call this to change any of the other settings (artifcat settings, "TV knob" settings, overscan, and screen settings). 
	* No allocations or graphics object creation happen in this function.
	* If you're using any settings other than the defaults, you'll want to call this at least once before you begin rendering
* **SetOutputSize**: This should be called whenever the output resolution changes (i.e. the window size or screen resolution).
	* This will reallocate some internal render targets to match the screen size
	* **This function must be called at least once before `Render` is called**
* **Render**: This should be called once per frame to render the NTSC effect
	* Takes an RGB `CathodeRetro::ITexture` as the input - the dimensions of this should match the width/height that were specified in the constructor or `UpdateSourceSettings`
	* The `scanlineType` parameter specifies whether this is an "even" or "odd" frame, for interlaced frames, or whether it's a "progressive" image (not interlaced)
	* This function will first call `BeginRendering` on the supplied `IGraphicsDevice`
	* After that comes the actual rendering, which will call `Update` on any used `IConstantBuffer` objects, as well as a number of `IGraphicsDevice::RenderQuad` calls
	* Finally, it will call `EndRendering` to let the supplied `IGraphicsDevice` restore any state that it needs to.

## License

>You can check out the full license [here](https://github.com/DeadlyRedCube/cathode-retro/blob/main/LICENSE)

This project is licensed under the terms of the **MIT** license.
Attribution (credit) would be greatly appreciated. If you use this, let me know! I want to see your cool projects!
