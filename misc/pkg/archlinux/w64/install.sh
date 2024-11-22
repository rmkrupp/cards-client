#/bin/bash

#
# build and install the AUR packages needed to cross-compile
#

function fetch_build() {
    pacman -Qq "$1" && return
    git clone "https://aur.archlinux.org/$1" "$1.gen" || exit 1
    cd "$1.gen" || exit 1
    makepkg --log && \
        sudo pacman --noconfirm -U *.pkg.tar.zst || { cd .. ; exit 1 ; }
    cd ..
}

#cmake depends on...
fetch_build mingw-w64-environment
fetch_build mingw-w64-pkg-config

#xz depends on
fetch_build mingw-w64-configure

#glfw depends on...
fetch_build mingw-w64-cmake

#cards-client depends on...
fetch_build mingw-w64-glfw
fetch_build mingw-w64-xz
fetch_build mingw-w64-vulkan-headers
fetch_build mingw-w64-vulkan-icd-loader

