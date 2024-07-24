# Doxygen Customization

This Doxygen build uses
[doxygen-awesome-css](https://jothepro.github.io/doxygen-awesome-css/index.html).

## Custom Header

The header has been customized to include a light mode/dark mode toggle
button. This may need to be regenerated when new versions of Doxygen are
released.
[Directions](https://jothepro.github.io/doxygen-awesome-css/md_docs_2extensions.html#extension-dark-mode-toggle)

1. Generate a fresh header using Doxygen

    ```
    doxygen -w html header.html delete_me.html delete_me.css
    ```
2. Customized generated `header.html` by adding the following inside `<head>`

    ```html
    <script type="text/javascript" src="$relpath^doxygen-awesome-darkmode-toggle.js"></script>
    <script type="text/javascript">
        DoxygenAwesomeDarkModeToggle.init()
    </script>
    ```
