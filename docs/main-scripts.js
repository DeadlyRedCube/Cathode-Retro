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
}