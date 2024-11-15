#!/bin/bash
# File: all-builds-test.sh
# Part of cards-client <github.com/rmkrupp/cards-client>
#
# Copyright (C) 2024 Noah Santer <n.ed.santer@gmail.com>
# Copyright (C) 2024 Rebecca Krupp <beka.krupp@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

function build() {
    if ./configure.py $@ && ninja ; then
        echo "build '$@' passed"
    else
        echo "build '$@' failed"
    fi

}

build
build --disable-argp
build --ldflags="-Wl,--as-needed"
build --no-defer-pkg-config

build --build=release
build --disable-argp --build=release
build --ldflags="-Wl,--as-needed" --build=release
build --no-defer-pkg-config --build=release

#build --build=w64
#build --ldflags="-Wl,--as-needed" --build=w64
#build --no-defer-pkg-config --build=w64


# Clean up a bit
./configure.py && ninja -tclean
./configure.py --build=w64 && ninja -tclean
./configure.py
