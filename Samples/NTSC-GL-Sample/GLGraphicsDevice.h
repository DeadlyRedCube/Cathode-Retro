#pragma once

#include <assert.h>
#include "NTSCify/GraphicsDevice.h"

#include "GLHelpers.h"


struct Vertex
{
  float x, y;
};


// A "Constant Buffer" class for NTSCify: wrapper around a GL uniform buffer.
class GLConstantBuffer :public NTSCify::IConstantBuffer
{
public:
  GLConstantBuffer(size_t sizeIn)
    : size(sizeIn)
  {
    // Need to pad up to a multiple of 16 bytes.
    size_t sizePadded = (sizeIn + 15) & ~15;
    glGenBuffers(1, &handle);
    CheckGLError();
    data.resize(sizePadded);
  }


  ~GLConstantBuffer()
  {
    glDeleteBuffers(1, &handle);
  }

  void Update(const void *dataIn, size_t dataSize) override
  {
    assert(dataSize == size);
    // Copy the data that we wanted into our potentially-padded buffer
    memcpy(data.data(), dataIn, dataSize);

    // Now actually drop the data into our buffer.
    glBindBuffer(GL_UNIFORM_BUFFER, handle);
    glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(data.size()), data.data(), GL_DYNAMIC_DRAW);
    CheckGLError();
  }


  GLuint Handle()
  {
    return handle;
  }

private:
  GLuint handle = 0;
  size_t size = 0;
  std::vector<uint8_t> data;
};


// This is an NTSCify "shader" which is really a wrapper around a GL shader program.
class GLShader : public NTSCify::IShader
{
public:
  // Build a GLShader given a vertex shader handle and a path to the pixel shader.
  GLShader(GLuint vsHandle, const char *path)
  {
    GLuint fsHandle = CompileShaderFromFile(GL_FRAGMENT_SHADER, path);
    shaderProgramHandle = LinkShaderProgram(vsHandle, fsHandle, path);
    glDeleteShader(fsHandle);
    CheckGLError();
  }

  ~GLShader()
  {
    glDeleteProgram(shaderProgramHandle);
  }

  GLuint ShaderProgramHandle() const
  {
    return shaderProgramHandle;
  }

private:
  GLuint shaderProgramHandle;
};


// This is a Texture wrapper, which can potentially contain a "render target" (Frame Buffer Object)
class GLTexture : public NTSCify::ITexture
{
public:
  static GLTexture FromBackbuffer(uint32_t width, uint32_t height)
  {
    return {width, height};
  }

  GLTexture(
    uint32_t widthIn,
    uint32_t heightIn,
    uint32_t mipCountIn, // 0 means "all mip levels"
    NTSCify::TextureFormat formatIn,
    bool isRenderTarget,
    void *optionalInitialDataTexels)
    : width(widthIn)
    , height(heightIn)
    , mipCount(mipCountIn)
    , format(formatIn)
  {
    glGenTextures(1, &texHandle);
    CheckGLError();
    glBindTexture(GL_TEXTURE_2D, texHandle);
    CheckGLError();

    GLint internalFormat = 0;
    GLenum glformat = 0;
    GLenum type = 0;
    switch (format)
    {
    case NTSCify::TextureFormat::RGBA_Unorm8:
      internalFormat = GL_RGBA8;
      glformat = GL_RGBA;
      type = GL_UNSIGNED_BYTE;
      break;
    case NTSCify::TextureFormat::RGBA_Float32:
      internalFormat = GL_RGBA32F;
      glformat = GL_RGBA;
      type = GL_FLOAT;
      break;
    case NTSCify::TextureFormat::R_Float32:
      internalFormat = GL_R32F;
      glformat = GL_RED;
      type = GL_FLOAT;
      break;
    case NTSCify::TextureFormat::RG_Float32:
      internalFormat = GL_RG32F;
      glformat = GL_RG;
      type = GL_FLOAT;
      break;
    }

    // Initialize the image to the correct size (with the correct initial contents)
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, glformat, type, optionalInitialDataTexels);

    if (mipCount != 1)
    {
      if (mipCount == 0)
      {
        // Calculate how many mip levels we expect.
        mipCount = 1 + uint32_t(std::floor(std::log2(float(std::max(width, height)))));
      }

      // Generate the given mip levels.
      //  This could probably be more efficient when there's no initial data.
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipCount - 1);
      glGenerateTextureMipmap(texHandle);
    }

    if (isRenderTarget)
    {
      // For render targets, we want a FBO per mip level.
      fboHandles.resize(mipCount);
      glGenFramebuffers(mipCount, fboHandles.data());
      for (uint32_t i = 0; i < mipCount; i++)
      {
        glBindFramebuffer(GL_FRAMEBUFFER, fboHandles[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texHandle, i);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
          throw std::exception("Failed to complete framebuffer");
        }
      }

      // Clear our framebuffer binding, we're done.
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    CheckGLError();
  }


  ~GLTexture()
  {
    if (!fboHandles.empty())
    {
      glDeleteFramebuffers(GLsizei(fboHandles.size()), fboHandles.data());
    }

    glDeleteTextures(1, &texHandle);
  }


  uint32_t Width() const override
  {
    return width;
  }


  uint32_t Height() const override
  {
    return height;
  }


  uint32_t MipCount() const override
  {
    return mipCount;
  }


  NTSCify::TextureFormat Format() const override
  {
    return format;
  }

  GLuint FBOHandle(uint32_t level) const
  {
    return fboHandles[level];
  }

  GLuint TexHandle() const
  {
    return texHandle;
  }

private:
  GLTexture(uint32_t w, uint32_t h)
    : width(w)
    , height(h)
    , mipCount(1)
    , texHandle(0)
    , format(NTSCify::TextureFormat::RGBA_Unorm8)
    , fboHandles{{0}}
  {
  }

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t mipCount = 0;
  GLuint texHandle = 0;
  NTSCify::TextureFormat format = NTSCify::TextureFormat::RGBA_Unorm8;
  std::vector<GLuint> fboHandles;
};


class GLGraphicsDevice : public NTSCify::IGraphicsDevice
{
public:
  GLGraphicsDevice(HDC dcIn)
    : dc(dcIn)
  {
    // Start by setting up an intermediate context
    context = wglCreateContext(dc);
    if (context == nullptr)
    {
      throw std::exception("wglCreateContext failed");
    }

    if (!wglMakeCurrent(dc, context))
    {
      throw std::exception("wglMakeCurrent failed");
    }

    // Now that that's set, run InitializeGLHelpers which will load all of the GL 3.3 functions that we need to get this train rolling
    InitializeGLHelpers();

    // Now that that's done we can create our true context (With the correct ersion)
    int attribs[] =
    {
      WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
      WGL_CONTEXT_MINOR_VERSION_ARB, 3,
      0, 0,
    };

    // Now create an OpenGL 3.3 context
    context = wglCreateContextAttribsARB(dc, nullptr, attribs);
    if (context == nullptr)
    {
      throw std::exception("wglCreateContext failed");
    }

    if (!wglMakeCurrent(dc, context))
    {
      throw std::exception("wglMakeCurrent failed");
    }

    // With that done, we need to create our UV quad vertex buffer (just use two vertex triangles instead of springing for an index buffer)
    Vertex vertices[] =
    {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 1.0f},
      {0.0f, 0.0f},
      {1.0f, 1.0f},
      {0.0f, 1.0f},
    };

    glGenBuffers(1, &vertexBufferObject);

    // Now make a vertex array object using the buffer
    glGenVertexArrays(1, &vertexArrayObject);
    glBindVertexArray(vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    CheckGLError();

    // Finally compile the common vertex shader that every quad render uses.
    vertexShaderHandle = CompileShaderFromFile(GL_VERTEX_SHADER, "d:/programming/source/NTSCify/Shaders/BasicVertexShader.hlsl");
  }


  ~GLGraphicsDevice()
  {
    glDeleteShader(vertexShaderHandle);
    glDeleteVertexArrays(1, &vertexArrayObject);
    glDeleteBuffers(1, &vertexBufferObject);
    wglDeleteContext(context);
  }


  std::unique_ptr<NTSCify::ITexture> CreateRenderTarget(
    uint32_t width,
    uint32_t height,
    uint32_t mipCount, // 0 means "all mip levels"
    NTSCify::TextureFormat format) override
  {
    return std::make_unique<GLTexture>(width, height, mipCount, format, true, nullptr);
  }


  std::unique_ptr<NTSCify::ITexture> CreateTexture(
    uint32_t width,
    uint32_t height,
    NTSCify::TextureFormat format,
    void *initialDataTexels)
  {
    return std::make_unique<GLTexture>(width, height, 1, format, false, initialDataTexels);
  }


  std::unique_ptr<NTSCify::IConstantBuffer> CreateConstantBuffer(size_t size) override
  {
    return std::make_unique<GLConstantBuffer>(size);
  }


  std::unique_ptr<NTSCify::IShader> CreateShader(NTSCify::ShaderID id) override
  {
    struct SShaderStuff
    {
      const char *path;
      const char *textureNames[32];
    };


    constexpr SShaderStuff k_shaderInfo[]
    {
      // GL (pre 4.2) needs a mapping from uniform to binding, and so here we list the expected binding orders in order. It would have
      //  been nicer to iterate through them by querying the shader (which is possible) but naturally they show up in arbitrary orders,
      //  rather than the order that they were declared in the shader.
      { .path = "Shaders/Downsample2x.hlsl", .textureNames = { "g_sourceTexture" } },
      { .path = "Shaders/TonemapAndDownsample.hlsl", .textureNames = { "g_sourceTexture" } },
      { .path = "Shaders/GaussianBlur13.hlsl", .textureNames = { "g_sourceTex" } },

      { .path = "Shaders/SignalGeneration/GeneratePhaseTexture.hlsl", .textureNames = {} },
      { .path = "Shaders/SignalGeneration/RGBToSVideoOrComposite.hlsl", .textureNames = { "g_sourceTexture", "g_scanlinePhases"} },
      { .path = "Shaders/SignalGeneration/ApplyArtifacts.hlsl", .textureNames = { "g_sourceTexture" } },

      { .path = "Shaders/SignalDecode/CompositeToSVideo.hlsl", .textureNames = { "g_sourceTexture" } },
      { .path = "Shaders/SignalDecode/SVideoToRGB.hlsl", .textureNames = { "g_sourceTexture", "g_scanlinePhases"} },
      { .path = "Shaders/SignalDecode/FilterRGB.hlsl", .textureNames = { "g_sourceTexture" } },

      { .path = "Shaders/CRT/GenerateScreenTexture.hlsl", .textureNames = { "g_shadowMaskTexture" } },
      { .path = "Shaders/CRT/GenerateShadowMask.hlsl", .textureNames = {} },
      {
        .path = "Shaders/CRT/RGBToCRT.hlsl",
        .textureNames =
        {
          "g_currentFrameTexture",
          "g_previousFrameTexture",
          "g_screenMaskTexture",
          "g_diffusionTexture",
        }
      },
    };

    auto &info = k_shaderInfo[size_t(id)];
    auto l = std::make_unique<GLShader>(vertexShaderHandle, info.path);

    glUseProgram(l->ShaderProgramHandle());
    for (uint32_t i = 0; info.textureNames[i] != nullptr; i++)
    {
      auto location = glGetUniformLocation(l->ShaderProgramHandle(), info.textureNames[i]);
      // assert(location >= 0);
      if (location >= 0)
      {
        glUniform1i(location, i);
      }
    }
    glUseProgram(0);

    return l;
  }


  void BeginRendering() override
  {
    if (!wglMakeCurrent(dc, context))
    {
      throw std::exception("wglMakeCurrent failed");
    }

    // All of our quads use the same vertex array.
    glBindVertexArray(vertexArrayObject);
    CheckGLError();
  }


  void RenderQuad(
    NTSCify::IShader *ps,
    NTSCify::RenderTargetView output,
    std::initializer_list<NTSCify::ShaderResourceView> inputs,
    NTSCify::IConstantBuffer *constantBuffer) override
  {
    // Start rendering to the correct mip level of the given texture and set up the viewport properly.
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLTexture *>(output.texture)->FBOHandle(output.mipLevel));
    glViewport(
      0,
      0,
      std::max(output.texture->Width() >> output.mipLevel, 1U),
      std::max(output.texture->Height() >> output.mipLevel, 1U));

    // Bind our shaders
    auto programHandle = static_cast<GLShader *>(ps)->ShaderProgramHandle();
    glUseProgram(programHandle);

    // Set up our constants if we have any
    if (constantBuffer != nullptr)
    {
      glUniformBlockBinding(programHandle, 0, 0);
      glBindBufferBase(GL_UNIFORM_BUFFER, 0, static_cast<GLConstantBuffer *>(constantBuffer)->Handle());
    }

    // Set up each texture
    for (size_t i = 0; i < inputs.size(); i++)
    {
      auto &input = inputs.begin()[i];

      auto texHandle = static_cast<const GLTexture *>(input.texture)->TexHandle();
      glActiveTexture(GLenum(GL_TEXTURE0 + i));
      glBindTexture(GL_TEXTURE_2D, texHandle);
      switch (input.samplerType)
      {
      case NTSCify::SamplerType::LinearWrap:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (input.texture->MipCount() == 1) ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;

      case NTSCify::SamplerType::LinearClamp:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (input.texture->MipCount() == 1) ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
      }

      if (input.mipLevel >= 0)
      {
        // We specified a mip level so clamp our base and max to the given level
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, input.mipLevel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, input.mipLevel);
      }
      else
      {
        // Use every mip level available.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, input.texture->MipCount() - 1);
      }
    }

    // Set the active texture back to 0 now that we're done with all that.
    glActiveTexture(GL_TEXTURE0);

    // Finally, draw the quad.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CheckGLError();
  }


  void EndRendering() override
  {
    // Set our framebuffer back to the render target.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CheckGLError();
  }


private:
  HDC dc;
  HGLRC context;

  GLuint vertexBufferObject = 0;
  GLuint vertexArrayObject = 0;
  GLuint vertexShaderHandle = 0;
};