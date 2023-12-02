# Using the C++ Code

(More-detailed documentation of methods and parameters coming soon - for now you'll have to refer to the code)

To use the C++ code contained within `Include/CathodeRetro`:

First, ensure the `Your-Cathode-Retro-Repository-Path/Include` directory is in your project's include path.

The main class is `CathodeRetro::CathodeRetro` and is in `Include/CathodeRetro/CathodeRetro.h`. It, however, requires you to implement a set of classes derived from the interfaces in `Include/CathodeRetro/GraphicsDevice.h`, which is the interface that the class will use to load shaders, create render targets, and otherwise hook into your renderer.


## Implementing the required interfaces
To implement these classes, you'll need to
``#include "CathodeRetro/GraphicsDevice.h"``

Then you will need to implement classes derived from the interfaces in that file:
* **CathodeRetro::IGraphicsDevice**: This is the main interface that Cathode Retro uses to interact with the graphics device. It can create objects (render targets, constant buffers, shaders) and render. You'll need to implement the following methods:
	* **CreateRenderTarget**: Create a `CathodeRetro::IRenderTarget`-derived object representing a render target (or frame buffer object) with the given properties.
	*  **CreateConstantBuffer**: Create a `CathodeRetro::IConstantBuffer`-derived object that represents a block of bytes used as a constant buffer (or uniform buffer) to pass data to the shaders.
	* **BeginRendering**: This is called by the `CathodeRetro::CathodeRetro` class when it is beginning its rendering, and is where you should set up any render state that is going to be consistent across the whole pipeline (the vertex shader, blending mode, etc).
		* Cathode Retro specifically wants no alpha blending or testing enabled. 
		* Additionally, it expects floating-point textures to be able to use the full range of values, so if the API allows for truncating floating-point values to the 0..1 range on either shader output or sampling input, that should be disabled.
	* **RenderQuad**: This is called during rendering to render a full-target quad using a shader with the given `ShaderID`, to the given `IRenderTarget`, using a set of input `ITexture`s and an `IConstantBuffer`.
	* **EndRendering**: This is called when the `CathodeRetro::CathodeRetro` class is done rendering, and is where you should restore any render states necessary for the rest of your renderer to continue as normal.
	
* **CathodeRetro::IConstantBuffer**: This is a "constant buffer" (GL/Vulkan refer to these as "uniform buffers" - basically a data buffer to be handed to a shader. These will be fully updated every frame so it's valid for this to allocate GPU bytes out of a pool and update for graphics APIs that prefer that style of CPU -> GPU buffering. These may be updated by the `CathodeRetro::CathodeRetro` class more than once per frame. It contains the following method:
	* **Update**: Copy the given data bytes into the constant buffer so that it is ready for rendering.

* **CathodeRetro::IRenderTarget**: This is a wrapper around a render target (or frame buffer object), and is derived from the `CathodeRetro::ITexture` interface which contains the following methods:
	* **Width**: Get the width of this texture.
	* **Height**: Get the height of this texture.
	* **MipCount**: Get the number of mipmap levels this texture has.
	* **Format**: Returns the `CathodeRetro::TextureFormat` enum value that corresponds to the format of this texture.

You may additionally need to implement a non-render-target texture class that derives from `CathodeRetro::ITexture` to pass non-render-target textures (like, say, a CPU-updated texture for an emulator) into the `CathodeRetro::CathodeRetro::Render` method.

See the sample projects in the `Samples/` directory for reference implementations of these classes for Direct3D 11 and OpenGL 3.3 Core


## Using the Main CathodeRetro Class

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