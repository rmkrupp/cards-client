# cards-client

The graphical client for [cards](https://github.com/rmkrupp/cards).

This project has submodules. When you clone it run `git submodule update
--init`.

Build with `./configure.py --build=release && ninja`

## Dependencies

### Build

 - gcc etc.
 - ninja
 - python

### Runtime

 - vulkan (on ArchLinux, building requires `vulkan-devel`)
 - GLFW

Pass `--disable-argp` to `configure.py` to fall back to getopt. This is the
default if `--build=w64` is given.

OpenMP is required to build the `generate-dfield` tool. You can pass
`--disable-tool=generate-dfield` to avoid building this tool and therefore
avoid the OpenMP dependency.

## Project Goals

 - [ ] GUI to control game setup, options, etc. as well as settings, saving,
       exiting, so on

   - [ ] Smoothly launch servers and other processes as needed for the chosen
         play type

   - [ ] Facilities to ease networking (i.e. displaying IP, etc.)

 - [ ] Vulkan-based 3D graphics

   - [ ] Display text

   - [ ] Display cards as 3D objects with highly scaleable lineart pictures
         and text

   - [ ] Cards arranged on configurable tables with opponents

   - [ ] Cards animated as they are played/moved/destroyed/etc

   - [ ] Decks and other stacks displayed

   - [ ] Viewing your own and opponent's hands

   - [ ] Indicators for selecting (zones, cards, etc.) and targeting

   - [ ] Animations/graphics to show opponent's actions (e.g. moving cards,
         arrows for attacking/targeting)

   - [ ] Visual indication of life and energy

 - [ ] Configuration of graphical settings (e.g. resolution, effects, etc.)

 - [ ] Loading configuration and settings

  - [ ] Above, but for the server

 - [ ] Saves integration (and managing the behavior of this re: the server)

 - [ ] Sound (music and effects)

 - [ ] In-game controls

   - [ ] Basic game actions

   - [ ] Composing trigger conditions (Scratch-esque block based)

   - [ ] Ability to directly write commands

 - [ ] Optionally, display the direct output of the server (i.e. the
       "narration")

 - [ ] Manage card and font assets
