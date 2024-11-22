#/bin/bash

#
# remove the local directories created by install.sh
#

function clean() {
    rm -rf "$1.gen"
}

clean mingw-w64-cmake
clean mingw-w64-configure
clean mingw-w64-environment
clean mingw-w64-glfw
clean mingw-w64-pkg-config
clean mingw-w64-vulkan-headers
clean mingw-w64-vulkan-icd-loader
clean mingw-w64-xz
