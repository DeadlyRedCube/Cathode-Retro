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
  "CathodeRetro",
  "Internal",
  "RGBToCRT",
  "SignalDecoder",
  "SignalGenerator",
  "MaskType",
  "SamplerType",
  "ScanlineType",
  "ShaderID",
  "SignalType",
  "TextureFormat",
  "IConstantBuffer",
  "IGraphicsDevice",
  "IRenderTarget",
  "IShader",
  "ITexture",
  "ArtifactSettings",
  "Color",
  "SignalLevels",
  "SignalProperties",
  "OverscanSettings",
  "Preset",
  "RenderTargetView",
  "ScreenSettings",
  "ShaderResourceView",
  "SourceSettings",
  "TVKnobSettings",
  "Vec2",

  "AspectData",
  "CommonConstants",
  "ScreenTextureConstants",
  "RGBToScreenConstants",
  "GaussianBlurConstants",
  "ToneMapConstants",
];

function SyntaxHighlight(element, keywords)
{
  let tokens = [];
  const text = Array.from(element.textContent);
  let i = 0;
  while (i < text.length)
  {
    if (text[i] == '/' && text[i + 1] == '/')
    {
      // Comment! Snag the entire rest of the line.
      let token = "";
      while (i < text.length && !IsNewline(text[i]))
      {
        token = token.concat(text[i]);
        i++;
      }
      tokens.push([token, "comment"]);
    }
    else if (text[i] == '#')
    {
      let token = text[i];
      i++;
      while (i < text.length && IsIdentifierChar(text[i]))
      {
        token = token.concat(text[i]);
        i++;
      }
      tokens.push([token, "preprocessor"]);
    }
    else if (IsNum(text[i]))
    {
      // Number: Does not currently support hex.
      let token = "";
      while (i < text.length && IsNum(text[i]))
      {
        token = token.concat(text[i]);
        i++;
      }
      tokens.push([token, "number"]);
    }
    else if (IsAlphaOrUnderscore(text[i]))
    {
      // Identifier or keyword
      let token = "";
      while (i < text.length && IsIdentifierChar(text[i]))
      {
        token = token.concat(text[i]);
        i++;
      }
      tokens.push([token, "identifier"]);
    }
    else if (text[i] == '"')
    {
      // String!
      let token = text[i];
      i++;
      while (i < text.length && text[i] != '"')
      {
        token = token.concat(text[i]);
        i++;
      }

      if (i < text.length)
      {
        token = token.concat(text[i]);
        i++;
      }

      tokens.push([token, "string"]);
    }
    else if (IsWhitespace(text[i]))
    {
      let token = "";
      while (i < text.length && IsWhitespace(text[i]))
      {
        token = token.concat(text[i]);
        i++;
      }

      tokens.push([token, "whitespace"]);
    }
    else
    {
      let token = "";
      while (i < text.length
        && !IsIdentifierChar(text[i])
        && text[i] !== '"'
        && !IsWhitespace(text[i])
        && (text[i] != '/' || text[i + 1] != '/'))
      {
        token = token.concat(text[i]);
        i++;
      }

      tokens.push([token, "operator"]);
    }
  }

  let newHTML = "";
  for (token of tokens)
  {
    let type = token[1];
    const text = token[0];

    if (type === "identifier" && keywords.includes(text))
    {
      type = "keyword";
    }
    else if (type === "identifier" && crTypes.includes(text))
    {
      type = "cr-type";
    }

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
    "auto",
    "break",
    "case",
    "catch",
    "char",
    "class",
    "const",
    "continue",
    "default",
    "delete",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "final",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "namespace",
    "new",
    "operator",
    "override",
    "private",
    "protected",
    "public",
    "return",
    "sealed",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "template",
    "this",
    "throw",
    "try",
    "typedef",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "while",

    // Not really keywords but I want to highlight them as such
    "uint32_t",
  ];
  for (codeDef of document.querySelectorAll(".syntax-cpp"))
  {
    for (pre of codeDef.querySelectorAll("pre"))
    {
      SyntaxHighlight(pre, cppKeywords);
    }
  }
}