#/bin/bash

#
# uninstall the AUR packages installed by install.sh
#

sudo pacman --noconfirm -R mingw-w64-xz
sudo pacman --noconfirm -R mingw-w64-glfw
sudo pacman --noconfirm -R mingw-w64-vulkan-icd-loader
sudo pacman --noconfirm -R mingw-w64-vulkan-headers
sudo pacman --noconfirm -R mingw-w64-cmake
sudo pacman --noconfirm -R mingw-w64-pkg-config
sudo pacman --noconfirm -R mingw-w64-configure
sudo pacman --noconfirm -R mingw-w64-environment

