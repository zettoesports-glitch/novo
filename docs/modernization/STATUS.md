# Modernization Status

## Active target
OpenGL 4.6 renderer modernization for Main 5.2.

## Architectural constraint
The renderer layer must remain extensible to Vulkan and DirectX 11, but those backends are not active implementation targets yet.

## Current documentation commits
- Renderer roadmap created.
- NextMU integration notes created.
- Implementation checklist created.
- Documentation index created.

## Next action
Perform source-level discovery and then implement the first safe OpenGL 4.6 BMD/object render slice.
