function ToggleMenu()
{
  let sidebar = document.getElementById("sidebar-container");
  if (sidebar.classList.contains("opened"))
    { sidebar.classList.remove("opened"); }
  else
    { sidebar.classList.add("opened"); }
}

function OnLoad()
{
  // Do whatever needs to be done at page load time // https://docs.sonata-project.org/projects/SonataAdminBundle/en/4.x/reference/action_show/
  let e = document.getElementById("sidebar-button");
  e.onclick = ToggleMenu;
}