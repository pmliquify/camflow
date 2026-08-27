# CamFlow API Documentation

## UI Specifications

- Visual Design and Layout: `docs/ui_design/ui_design.md`
- React Architecture: `docs/ui_design/ui_react_design.md`
- Parameter Panel: `docs/ui_design/ui_parameter_panel_spec.md`
- Header: `docs/ui_design/ui_header_spec.md`
- Editor Panel: `docs/ui_design/ui_editor_panel_spec.md`
- Media Graph: `docs/ui_design/ui_media_graph_spec.md`
- Viewer Panel: `docs/ui_design/ui_viewer_panel_spec.md`
- Web UI Service and Development: `docs/development/web_ui.md`

CamFlow API documentation is generated with Doxygen and styled with doxygen-awesome.

## Tooling

- Doxygen: https://www.doxygen.nl/
- doxygen-awesome: https://github.com/jothepro/doxygen-awesome-css

The `tasks.sh docs` action downloads theme CSS files into docs/development/doxygen-awesome and applies them automatically.

## Generate Docs

```bash
# Install dependencies
./scripts/tasks.sh docs --install-deps

# Clean old output and regenerate
./scripts/tasks.sh docs --clean

# Generate to custom output folder
./scripts/tasks.sh docs --output /tmp/camflow-docs
```

## Requirements

Linux:

```bash
sudo apt update
sudo apt install -y doxygen graphviz curl
```

macOS:

```bash
brew install doxygen graphviz curl
```

## Output

Default output path:

- docs/development/api

Theme assets path:

- docs/development/doxygen-awesome

## Comment Style

Use Doxygen-style comments in C++ headers and sources:

```cpp
/**
 * @brief One-line summary of the class.
 *
 * Detailed description of behavior and design intent.
 *
 * @param value Parameter description.
 * @return Return value description.
 * @see RelatedType
 */
class Example
{
public:
    /**
     * @brief Performs work.
     * @param input Input value.
     * @return True on success.
     */
    bool run(int input);
};
```

Common tags:

- @brief
- @param
- @return
- @throws
- @see
- @note
- @warning
- @deprecated
- @code / @endcode

## Notes

- The script uses source roots under src.
- Graphviz is enabled for class and dependency diagrams.
- Main page is derived from README.md.
