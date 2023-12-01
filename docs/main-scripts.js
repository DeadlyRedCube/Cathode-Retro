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
}