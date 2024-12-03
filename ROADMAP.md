# Roadmap to 1.0

## Version 0.1

Basic rendering stuff

 - [x] textures, samplers
 - [x] depth buffering
 - [ ] mipmaps
 - [x] multisampling
 - [x] push constants
 - [ ] extra debug messages
 - [x] signed distance maps

Abstractions

 - [ ] models, model sets
 - [x] camera
 - [ ] time
 - [x] scheduled movements and interpolations etc.

More advanced stuff

 - [ ] smarter multi-object rendering and lifetimes of objects etc.
 - [ ] VMA

Misc

 - [ ] Review multithreading possiblities
 - [ ] https://developer.nvidia.com/blog/vulkan-dos-donts/

Packaging and w64 stuff

 - [x] set up standalone builds
 - [x] test w64 builds

## Tech Demo

 - [x] multiple objects
 - [x] scenes
   - [x] scene scripts
   - [x] timelines
   - [ ] API
   - [ ] animations / loops
   - [ ] transitions
 - [x] cameras
   - [x] queued movements
 - [ ] rain effects
 - [ ] glow
 - [ ] OIT
 - [ ] ambient lighting
 - [ ] point lighting
 - [ ] shadow casting
 - [ ] reflections / glass
 - [ ] stick figure / connected object / floating head
 - [ ] text
   - [ ] timed, placed, moving text
 - [ ] audio
   - [ ] background
   - [ ] positional
 - [ ] some configurables
