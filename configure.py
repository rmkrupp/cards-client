#!/usr/bin/python3
# File: configure.py
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

import argparse
import os
import sys
import subprocess
from datetime import datetime
import misc.ninja_syntax as ninja

#
# DIRECTLY INVOKED PROGRAMS
#

# used only if --defer-pkg-config=false
pkgconfig = {
        'release': 'pkg-config',
        'debug': 'pkg-config',
        'w64': 'x86_64-w64-mingw32-pkg-config',
        'w64-debug': 'x86_64-w64-mingw32-pkg-config'
    }

#
# ARGUMENT PARSING
#

parser = argparse.ArgumentParser(
        prog = 'configure.py',
        description = 'generate the build.ninja file',
        epilog = 'This program is part of cards-client <github.com/rmkrupp/cards-client>'
    )

parser.add_argument('--cflags', help='override compiler flags (and CFLAGS)')
parser.add_argument('--ldflags',
                    help='override compiler flags when linking (and LDFLAGS')

parser.add_argument('--cc', help='override cc (and CC)')
parser.add_argument('--glslc', help='override glslc (and GLSLC)')
parser.add_argument('--pkg-config', help='override pkg-config')

parser.add_argument('--build',
                    choices=['release', 'debug', 'w64', 'w64-debug'], default='debug',
                    help='set the build type (default: debug)')
parser.add_argument('--enable-compatible', action='store_true',
                    help='enable compatibility mode for older compilers')
parser.add_argument('--disable-sanitize', action='store_true',
                    help='don\'t enable the sanitizer in debug mode')
parser.add_argument('--vulkan-version', default='auto',
                    choices=['auto', '1.0', '1.1', '1.2', '1.3'],
                    help='TODO')
parser.add_argument('--build-native',
                    choices=['none', 'mtune', 'march', 'both'], default='none',
                    help='build with mtune=native or march=native')
parser.add_argument('--O3', '--o3', action='store_true',
                    help='build releases with -O3')

parser.add_argument('--disable-test-tool', action='append', default=[],
                    choices=[
                    ],
                    help='don\'t build a specific test tool')
parser.add_argument('--disable-tool', action='append', default=[],
                    choices=[
                        'generate-dfield'
                    ],
                    help='don\'t build a specific tool')
parser.add_argument('--disable-client', action='store_true',
                    help='don\'t build the client')

parser.add_argument('--disable-argp', action='store_true',
                    help='fall back to getopt for argument parsing')

parser.add_argument('--force-version', metavar='STRING',
                    help='override the version string')
parser.add_argument('--add-version-suffix', metavar='SUFFIX',
                    help='append the version string')

# n.b. we aren't using BooleanOptionalAction for Debian reasons
parser.add_argument('--defer-git-describe', action='store_true', default=True,
                    help='run git describe when ninja is run, not configure.py (default)')
parser.add_argument('--no-defer-git-describe', action='store_false', dest='defer_git_describe',
                    help='run git describe when configure.py is run, not ninja')
parser.add_argument('--defer-pkg-config', action='store_true', default=True,
                    help='run pkg-config when ninja is run, not configure.py (default)')
parser.add_argument('--no-defer-pkg-config', action='store_false', dest='defer_pkg_config',
                    help='run pkg-config when configure.py is run, not ninja')


args = parser.parse_args()

#
# HELPER FUNCTIONS
#

def package(name,
            alias=None,
            pkg_config=True,
            libs={},
            cflags={},
            comment=True):
    libs = (libs.get(args.build, '') + ' ' + libs.get('all', '')).strip()
    if len(libs) > 0:
        libs = ' ' + libs

    cflags = (cflags.get(args.build, '') + ' ' + cflags.get('all', '')).strip()
    if len(cflags) > 0:
        cflags = ' ' + cflags

    if not alias:
        alias = name

    if comment:
        w.comment('package ' + name) 
    if pkg_config:
        if args.defer_pkg_config:
            w.variable(alias + '_cflags', '$$($pkgconfig --cflags ' + name + ')')
            w.variable(alias + '_libs', '$$($pkgconfig --libs ' + name + ')')
        else:
            if not args.pkg_config and args.build not in pkgconfig:
                print('ERROR: --defer-pkg-config is false but there is no pkg-config for build type', args.build, '(have:', pkgconfig, ')', file=sys.stderr)
                sys.exit(1)

            if args.pkg_config:
                pc = args.pkg_config
            else:
                pc = pkgconfig.get(args.build)
            pc_cflags = subprocess.run([pc, '--cflags', name],
                                       capture_output=True)
            pc_libs = subprocess.run([pc, '--libs', name],
                                     capture_output=True)
            if pc_cflags.returncode != 0 or pc_libs.returncode != 0:
                w.comment(
                        'WARNING: ' + pc +
                        ' exited non-zero for library ' + name
                    )
                print('WARNING:', pc, 'exited non-zero for library',
                      name, file=sys.stderr)
            w.variable(alias + '_cflags',
                       pc_cflags.stdout.decode().strip() + cflags)
            w.variable(alias + '_libs',
                       pc_libs.stdout.decode().strip() + libs)
    else:
        w.variable(alias + '_cflags', cflags)
        w.variable(alias + '_libs', libs)
    w.newline()

def transformer(source, rule):
    if rule == 'cc':
        if source[-2:] == '.c':
            return source[:-2] + '.o'
    elif rule == 'glslc':
        if source[-5:] == '.glsl':
            return source[:-5] + '.spv'
    return source

def build(source,
          rule='cc',
          input_prefix='src/',
          output_prefix='$builddir/',
          packages=[],
          cflags='$cflags',
          includes='$includes',
          stage=None):

    variables = []
    cflags = ' '.join([cflags] + ['$' + name + '_cflags' for name in packages])
    if cflags != '$cflags':
        variables += [('cflags', cflags)]

    if includes != '$includes':
        variables += [('includes', includes)]

    if stage:
        variables += [('stage', stage)]

    w.build(
            output_prefix + transformer(source, rule),
            rule,
            input_prefix + source,
            variables=variables
        )


def exesuffix(root, enabled):
    if enabled:
        return root + '.exe'
    else:
        return root

def enable_debug():
    if args.enable_compatible:
        w.variable(key = 'std', value = '-std=gnu2x')
        w.variable(key = 'cflags', value = '$cflags $sanflags -g3 -Og')
        w.comment('adding compatibility defines because we were generated with --enable-compatible')
        w.variable(key = 'defines',
                   value = '$defines '+
                   '-Dconstexpr=const ' +
                   '"-Dstatic_assert(x)=" ' +
                   '-DENABLE_COMPAT')
    else:
        w.variable(key = 'std', value = '-std=gnu23')
        w.variable(key = 'cflags', value = '$cflags $sanflags -g3 -Og')

    if not args.force_version:
        w.variable(key = 'version', value = '"$version"-debug')
    else:
        w.comment('not appending -debug because we were generated with --force-version=')

def enable_release():
    if args.O3:
        w.comment('setting -O3 because we were generated with --O3')
        o = '-O3'
    else:
        o = '-O2'

    if args.enable_compatible:
        w.variable(key = 'std', value = '-std=gnu2x')
        w.variable(key = 'cflags', value = '$cflags ' + o)
        w.comment('adding compatibility defines because we were generated with --enable-compatible')
        w.variable(key = 'defines',
                   value = '$defines '+
                   '-Dconstexpr=const ' +
                   '"-Dstatic_assert(x)=" ' +
                   '-DENABLE_COMPAT')
    else:
        w.variable(key = 'std', value = '-std=gnu23')
        w.variable(key = 'cflags', value = '$cflags ' + o)

    w.variable(key = 'defines', value = '$defines -DNDEBUG')

def enable_w64():
    if args.O3:
        w.comment('setting -O3 because we were generated with --O3')
        o = '-O3'
    else:
        o = '-O2'
    args.disable_argp = True
    w.variable(key = 'cflags', value = '$cflags -static ' + o)
    w.variable(key = 'windows', value = '-lgdi32 -mwindows')
    if args.enable_compatible:
        w.comment('adding compatibility defines because we were generated with --enable-compatible')
        w.variable(key = 'std', value = '-std=gnu2x')
        w.variable(key = 'defines',
                   value = '$defines '+
                   '-Dconstexpr=const ' +
                   '"-Dstatic_assert(x)=" ' +
                   '-DENABLE_COMPAT')
    else:
        w.variable(key = 'std', value = '-std=gnu23')

    w.variable(key = 'includes',
               value = '$includes -I/usr/x86_64-w64-mingw32/include')
    w.variable(key = 'defines', value = '$defines -DNDEBUG')

def enable_w64_debug():
    args.disable_argp = True
    w.variable(key = 'cflags', value = '$cflags -static -static-libubsan -g3 -Og')
    w.variable(key = 'windows', value = '-lgdi32 -mwindows')
    if args.enable_compatible:
        w.comment('adding compatibility defines because we were generated with --enable-compatible')
        w.variable(key = 'std', value = '-std=gnu2x')
        w.variable(key = 'defines',
                   value = '$defines '+
                   '-Dconstexpr=const ' +
                   '"-Dstatic_assert(x)=" ' +
                   '-DENABLE_COMPAT')
    else:
        w.variable(key = 'std', value = '-std=gnu23')

    w.variable(key = 'includes',
               value = '$includes -I/usr/x86_64-w64-mingw32/include')

    if not args.force_version:
        w.variable(key = 'version', value = '"$version"-debug')
    else:
        w.comment('not appending -debug because we were generated with --force-version=')

#
# THE WRITER
#

w = ninja.Writer(open('build.ninja', 'w'))

#
# PREAMBLE
#

w.comment('we were generated by configure.py on ' + datetime.now().strftime('%d-%m-%y %H:%M:%S'))
w.comment('arguments: ' + str(sys.argv[1:]))
w.newline()

#
# BASE VERSION
#

if args.force_version:
    # this option also disables the -debug suffix in enable_debug()
    # but it does not disable --add_version_suffix
    w.comment('the following version was set at generation by --force-version=' + args.force_version)
    w.variable(key = 'version', value = args.force_version)
else:
    w.variable(key = 'version', value = '$$(git describe --always --dirty)')

w.variable(key = 'builddir', value = 'out')

#
# TOOLS TO INVOKE
#

def warn_environment(key, argsval, flagname):
    if key in os.environ and argsval:
        print('WARNING:', key,
              'environment variable is set but will be ignored because',
              flagname, 'was passed', file = sys.stderr)
        w.comment('WARNING: ' + key +
                  ' environment variable is set but will be ignored because ' +
                  flagname + ' was passed')

warn_environment('CC', args.cc, '--cc=')
warn_environment('GLSLC', args.cc, '--glslc=')
# TODO: is there a normal environment variable for pkg-config?
#warn_environment('PKG_CONFIG', args.cc, '--pkg-config=')
warn_environment('CFLAGS', args.cc, '--cflags=')
warn_environment('LDFLAGS', args.cc, '--ldflags=')

if args.cc:
    if (args.build[:3] == 'w64' and args.cc != 'x86_64-w64-mingw32-gcc') \
            or (args.build[:3] != 'w64' and args.cc != 'gcc'):
        w.comment('using this cc because we were generated with --cc=' +
                  args.cc)
    w.variable(key = 'cc', value = args.cc)
elif 'CC' in os.environ:
    w.comment('using this cc because CC was set')
    w.variable(key = 'cc', value = os.environ['cc'])
elif args.build == 'w64' or args.build == 'w64-debug':
    w.variable(key = 'cc', value = 'x86_64-w64-mingw32-gcc')
else:
    w.variable(key = 'cc', value = 'gcc')

if args.glslc:
    if args.glslc != 'glslc':
        w.comment('using this glslc because we were generated with --glslc=' +
                  args.glslc)
    w.variable(key = 'glslc', value = args.glslc)
elif 'GLSLC' in os.environ:
    w.comment('using this glslc because GLSLC was set')
else:
    w.variable(key = 'glslc', value = 'glslc')

if args.pkg_config:
    w.comment('using this pkg-config because we were generated with --pkg-config=' +
              args.pkg_config)
    w.variable(key = 'pkgconfig', value = args.pkg_config)
elif args.build == 'w64' or args.build == 'w64-debug':
    w.variable(key = 'pkgconfig', value = 'x86_64-w64-mingw32-pkg-config')
else:
    w.variable(key = 'pkgconfig', value = 'pkg-config')

#
# CFLAGS/LDFLAGS DEFAULTS
#

if args.cflags:
    w.comment('these are overriden below because we were generated with --cflags=' +
              args.cflags)
elif 'CFLAGS' in os.environ:
    w.comment('these are overriden below because CFLAGS was set')

w.variable(key = 'cflags',
           value = '-Wall -Wextra -Werror -fdiagnostics-color -flto=auto -D_FORTIFY_SOURCE=2 -fopenmp')

if args.ldflags:
    w.comment('these are overriden below because we were generated with --ldflags=' +
              args.ldflags)
elif 'LDFLAGS' in os.environ:
    w.comment('these are overriden below because LDFLAGS was set')

w.variable(key = 'ldflags', value = '')

#
# MTUNE/MARCH SETTINGS
#

if args.build_native == 'none':
    pass
elif args.build_native == 'mtune':
    w.comment('# adding cflags for --build-native=mtune')
    w.variable(key = 'cflags', value = '$cflags -mtune=native')
elif args.build_native == 'march':
    w.comment('# adding cflags for --build-native=march')
    w.variable(key = 'cflags', value = '$cflags -march=native')
elif args.build_native == 'both':
    w.comment('# adding cflags for --build-native=both')
    w.variable(key = 'cflags', value = '$cflags -march=native -mtune=native')
else:
    print('WARNING: unhandled build-native mode "' + args.build_native + '"', file=sys.stderr)
    w.comment('WARNING: unhandled build-native mode "' + args.build_native +'"')

#
# SANITIZER
#

if args.build == 'w64':
    w.comment('-fsanitize disabled for w64 builds')
    w.variable(key = 'sanflags', value = '')
elif args.disable_sanitize:
    w.comment('-fsanitize disabled because we were generated with --disable-sanitize')
    w.variable(key = 'sanflags', value = '')
else:
    w.variable(key = 'sanflags', value = '-fsanitize=address,undefined')

#
# INCLUDES
#

w.variable(key = 'includes', value = '-Iinclude -Ilibs/quat/include')
w.newline()

#
# BUILD MODE
#

if args.build == 'debug':
    if args.O3:
        print('WARNING: ignoring option --O3 for debug build', file=sys.stderr)
        w.comment('WARNING: ignoring option --O3 for debug build')
    w.comment('build mode: debug')
    enable_debug()
elif args.build == 'release':
    w.comment('build mode: release')
    enable_release()
elif args.build == 'w64':
    w.comment('build mode: w64')
    w.comment('(this implies --disable-argp)')
    enable_w64()
elif args.build == 'w64-debug':
    w.comment('build mode: w64-debug')
    w.comment('(this implies --disable-argp)')
    enable_w64_debug()
else:
    print('WARNING: unhandled build mode "' + args.build + '"', file=sys.stderr)
    w.comment('WARNING: unhandled build mode "' + args.build +'"')
w.newline()

#
# CFLAGS/LDFLAGS OVERRIDES
#

needs_newline = False

if args.cflags:
    w.variable(key = 'cflags', value = args.cflags)
    needs_newline = True

if args.ldflags:
    w.variable(key = 'ldflags', value = args.ldflags)
    needs_newline = True

#
# OPTIONAL VERSION SUFFIX
#

w.comment('the version define')

if args.add_version_suffix:
    w.variable(key = 'version', value = '"$version"-' + args.add_version_suffix)
    needs_newline = True

if needs_newline:
    w.newline()

#
# THE VERSION DEFINE
#

w.variable(key = 'defines', value = '$defines -DVERSION="\\"$version\\""')
w.newline()

#
# PACKAGES
#

package('vulkan')
package('glfw3')
package('liblzma', alias='lzma')

#
# NINJA RULES
#

w.rule(
        name = 'cc',
        deps = 'gcc',
        depfile = '$out.d',
        command = '$cc $std $includes -MMD -MF $out.d $defines ' +
                  '$cflags $in -c -o $out'
    )
w.newline()

w.rule(
        name = 'bin',
        deps = 'gcc',
        depfile = '$out.d',
        command = '$cc $std $includes -MMD -MF $out.d $defines ' +
                  '$cflags $in -o $out $ldflags $libs'
    )
w.newline()

w.rule(
        name = 'glslc',
        deps = 'gcc',
        depfile = '$out.d',
        command = '$glslc -Werror -MD -MF $out.d $glsldefines ' +
                  '-fshader-stage=$stage $glslflags $in -o $out'
    )
w.newline()

#
# SOURCES
#

w.comment('source files')

build('main.c')
build('dfield.c', cflags='$cflags -fopenmp', packages=['lzma'])
w.newline()

build('renderer/renderer.c', packages=['vulkan', 'glfw3'])
build('renderer/scene.c', packages=['vulkan', 'glfw3'])
w.newline()

build('util/sorted_set.c')
build('util/strdup.c')
w.newline()

build('tools/generate-dfield/generate-dfield.c')
build('tools/generate-dfield/args_argp.c',
      cflags='$cflags -Wno-missing-field-initializers')
build('tools/generate-dfield/args_getopt.c')
w.newline()

build('quat.c',
      input_prefix='libs/quat/src/', output_prefix='$builddir/libs/quat/')
w.newline()

build('shaders/vertex.glsl', rule='glslc', stage='vertex')
build('shaders/fragment.glsl', rule='glslc', stage='fragment')
w.newline()

#
# OUTPUTS
#

w.comment('output products')

all_targets = []
tools_targets = []

def bin_target(name,
               inputs,
               implicit_inputs=[],
               argp_inputs=[],
               getopt_inputs=[],
               targets=[],
               variables=[],
               is_disabled=False,
               why_disabled=''):
    fullname = exesuffix(name, args.build == 'w64')

    if type(is_disabled) == bool:
        is_disabled = [is_disabled]
        why_disabled = [why_disabled]

    assert(len(is_disabled) == len(why_disabled))

    if True not in is_disabled:
        if args.disable_argp and (len(argp_inputs) > 0 or len(getopt_inputs) > 0):
            w.comment('# building ' + name + ' with getopt because we were generated with --disable-argp')
            inputs += getopt_inputs
        else:
            inputs += argp_inputs

        for group in targets:
            group += [fullname]
        w.build(
                fullname,
                'bin',
                inputs,
                implicit=implicit_inputs,
                variables=variables
            )
    else:
        if sum([1 for disabled in is_disabled if disabled]) > 1:
            w.comment(fullname + ' is disabled because:')
            for disabled, why in zip(is_disabled, why_disabled):
                if disabled:
                    w.comment(' - ' + why)
        else:
            w.comment(fullname + ' is disabled because ' +
                      [why_disabled[x] for x in range(len(why_disabled))
                       if is_disabled[x]][0])
    w.newline()

bin_target(
        name = 'cards-client',
        inputs = [
            '$builddir/main.o',
            '$builddir/renderer/renderer.o',
            '$builddir/renderer/scene.o',
            '$builddir/dfield.o',
            '$builddir/util/sorted_set.o',
            '$builddir/util/strdup.o',
            '$builddir/libs/quat/quat.o'
        ],
        implicit_inputs = [
            '$builddir/shaders/vertex.spv',
            '$builddir/shaders/fragment.spv'
        ],
        variables = [
            ('libs', '-lm $vulkan_libs $glfw3_libs $lzma_libs -fopenmp $windows')
        ],
        is_disabled = args.disable_client,
        why_disabled = 'we were generated with --disable-client',
        targets = [all_targets]
    )

bin_target(
        name = 'tools/generate-dfield',
        inputs = [
            '$builddir/tools/generate-dfield/generate-dfield.o',
            '$builddir/dfield.o',
            '$builddir/util/strdup.o'
        ],
        argp_inputs = [
            '$builddir/tools/generate-dfield/args_argp.o'
        ],
        getopt_inputs = [
            '$builddir/tools/generate-dfield/args_getopt.o'
        ],
        variables = [
            ('libs', '-lm -fopenmp $lzma_libs')
        ],
        is_disabled = 'generate-dfield' in args.disable_tool,
        why_disabled = 'we were generated with --disable-tool=generate-dfield',
        targets = [all_targets, tools_targets]
    )

#
# ALL, TOOLS, AND DEFAULT
#

if len(tools_targets) > 0:
    w.build('tools', 'phony', tools_targets)
else:
    w.comment('NOTE: no tools target because there are no enabled tools')
w.newline()

w.build('all', 'phony', all_targets)
w.newline()

w.default('all')

#
# DONE
#

w.close()
