function ToggleMenu()
{
  let sidebar = document.getElementById("sidebar-container");
  if (sidebar.classList.contains("opened"))
  {
    sidebar.classList.remove("opened"); 
    document.body.classList.remove("sidebar-opened");
  }
  else
  { 
    sidebar.classList.add("opened"); 
    document.body.classList.add("sidebar-opened");
  }
}

function IsAlphaOrUnderscore(c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

function IsNum(c)
{
  return (c >= '0' && c <= '9');
}

function IsIdentifierChar(c)
{
  return IsNum(c) || IsAlphaOrUnderscore(c);
}

function IsNewline(c)
{
  return c === '\n' || c === '\r';
}

function IsWhitespace(c)
{
  return c === ' ' || c === '\t' || IsNewline(c);
}

const crTypes =
[
  "CathodeRetro", "Internal", "RGBToCRT", "SignalDecoder", "SignalGenerator", "MaskType", "SamplerType", "ScanlineType", "ShaderID",
  "SignalType", "TextureFormat", "IConstantBuffer", "IGraphicsDevice", "IRenderTarget", "ITexture", "ArtifactSettings", "Color",
  "SignalLevels", "SignalProperties", "OverscanSettings", "Preset", "RenderTargetView", "ScreenSettings", "ShaderResourceView", "SourceSettings", "TVKnobSettings",
  "Vec2",

  "AspectData", "CommonConstants", "ScreenTextureConstants", "RGBToScreenConstants", "GaussianBlurConstants", "ToneMapConstants",
  "CompositeToSVideoConstantData", "SVideoToModulatedChromaConstantData", "SVideoToRGBConstantData", "FilterRGBConstantData",
];

function SyntaxHighlight(element, keywords)
{
  let matches = [...element.textContent.matchAll(/(\/\/[^\r\n]*)|(#[\A-Za-z_]*)|("[^"]*")|([A-Za-z_][\w]*)|(\d+)(?:\.\d*f?)?|(\s+)|([!@%^&*\(\)~{}\[\]\|:;\.\,=\-\>\<\?\/]+)/g)];
  let newHTML = "";
  for (v of matches)
  {
    const text = v[0];
    let type="operator";
    if (v[1] !== undefined)
      type = "comment";
    else if (v[2] !== undefined)
      type = "preprocessor";
    else if (v[3] !== undefined)
      type = "string";
    else if (v[4] !== undefined)
      type = keywords.includes(text) ? "keyword" : crTypes.includes(text) ? "cr-type" : "identifier";
    else if (v[5] !== undefined)
      type = "number";
    else if (v[6] !== undefined)
      type = "whitespace";
    else
      console.assert(v[7] !== undefined);

    newHTML = newHTML.concat(`<span class="syntax-${type}">${text}</span>`);
  }

  element.innerHTML = newHTML;
}

function OnLoad()
{
  // Do whatever needs to be done at page load time // https://docs.sonata-project.org/projects/SonataAdminBundle/en/4.x/reference/action_show/
  let e = document.getElementById("sidebar-button");
  e.onclick = ToggleMenu;

  // On safari hitting "back" can re-show the page as it was, which means we need to explicitly close the sidebar if it was still open.
  document.body.onpageshow = () =>
  {
    let sidebar = document.getElementById("sidebar-container");
    sidebar.classList.remove("opened");
    document.body.classList.remove("sidebar-opened");
  };

  for (codeDef of document.querySelectorAll(".code-definition"))
  {
    for (pre of codeDef.querySelectorAll("pre"))
    {
      // Figure out the indentation of the first line and remove that many spaces from all subsequent lines.
      let text = pre.textContent.trimEnd().replaceAll("\r\n", "\n").replaceAll("\r", "\n").replace(/^\n+/, "");
      let spaces = text.match(/^[ ]*/)[0];
      pre.textContent = text.substring(spaces.length).replaceAll(`\n${spaces}`, "\n");
    }
  }

  const cppKeywords =
  [
    "auto", "bool", "break", "case", "catch", "char", "class", "const", "continue", "default",
    "delete", "do", "double", "else", "enum", "extern", "false", "final", "float", "for",
    "friend", "goto", "if", "inline", "int", "long", "namespace", "new", "nullptr", "operator",
    "override", "private", "protected", "public", "return", "sealed", "short", "signed", "sizeof",
    "static", "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typename", "union",
    "unsigned", "using", "virtual", "void", "volatile", "while",

    // Not really keywords but I want to highlight them as such
    "uint32_t", "size_t",
  ];

  const hlslKeywords =
  [
    "int", "int2", "int3", "int4",
    "uint", "uint2", "uint3", "uint4",
    "float", "float2", "float3", "float4",
    "return", "void", "out",
  ];

  for (codeDef of document.querySelectorAll(".syntax-cpp"))
  {
    for (pre of codeDef.querySelectorAll("pre"))
    {
      SyntaxHighlight(pre, cppKeywords);
    }
  }

  for (codeDef of document.querySelectorAll(".syntax-hlsl"))
  {
    for (pre of codeDef.querySelectorAll("pre"))
    {
      SyntaxHighlight(pre, hlslKeywords);
    }
  }
}