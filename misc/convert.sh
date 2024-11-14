#!/bin/bash
# File: misc/convert.sh
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

if [ $# -lt 2 ] ; then
    echo "Syntax: $0 INPUT_SVG OUTPUT_SIZE"
    exit 1
fi

#
# TODO: this shouldn't always be square
# TODO: we should support creating multiple resolutions from each input
#

in=4098
out="$2"
spread="$2"
dir="$(mktemp -d)"
mkdir -p "$dir/$(dirname $1)"
outdir="out/$(dirname "$1")/$2"
outbase="$(basename "$1")"
mkdir -p "$outdir"
inkscape -C -o "$dir/${1%.svg}.png" -w "$in" -h "$in" "$1" || exit 1
magick "$dir/${1%.svg}.png" -transparent "#FFFFFFFF" -alpha Extract \
    "gray:$dir/${1%.svg}.dat" || exit 1
./tools/generate-dfield -I "$in" -O "$out" -S "$spread" \
    "$outdir/${outbase%.svg}.dfield" "$dir/${1%.svg}.dat" || exit 1
#magick -depth 8 -size "$2x$2" "gray:$outdir/${outbase%.svg}.dfield" "$outdir/${outbase%.svg}.png"

