#!/bin/sh

#
# Copyright (c) 2018 Vojtech Horky
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# - The name of the author may not be used to endorse or promote products
#   derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# This is wrapper script for testing build of HelenOS under Travis CI [1].
#
# You probably do not want to run this script directly. If you wish to test
# that HelenOS builds for all architectures, consider using either check.sh
# script or running our CI solution [2] (if you want to test ported software
# too).
#
# [1] https://travis-ci.org/
# [2] https://www.helenos.org/wiki/CI
#

H_ARCH_CONFIG_CROSS_TARGET=2
H_ARCH_CONFIG_OUTPUT_FILENAME=3

h_get_arch_config_space() {
    cat <<'EOF_CONFIG_SPACE'
amd64:amd64-helenos:image.iso
arm32/beagleboardxm:arm-helenos:uImage.bin
arm32/beaglebone:arm-helenos:uImage.bin
arm32/gta02:arm-helenos:uImage.bin
arm32/integratorcp:arm-helenos:image.boot
arm32/raspberrypi:arm-helenos:uImage.bin
arm64/virt:aarch64-helenos:image.iso
ia32:i686-helenos:image.iso
ia64/i460GX:ia64-helenos:image.boot
ia64/ski:ia64-helenos:image.boot
mips32/malta-be:mips-helenos:image.boot
mips32/malta-le:mipsel-helenos:image.boot
mips32/msim:mipsel-helenos:image.boot
ppc32:ppc-helenos:image.iso
sparc64/niagara:sparc64-helenos:image.iso
sparc64/ultra:sparc64-helenos:image.iso
EOF_CONFIG_SPACE
}

h_get_arch_config() {
    h_get_arch_config_space | grep "^$H_ARCH:" | cut '-d:' -f "$1"
}

H_DEFAULT_HARBOURS_LIST="binutils fdlibm jainja libgmp libiconv msim pcc zlib libisl libmpfr libpng python2 libmpc gcc"



#
# main script starts here
#

# Check we are actually running inside Travis
if [ -z "$TRAVIS" ]; then
    echo "\$TRAVIS env not set. Are you running me inside Travis?" >&2
    exit 5
fi

# C style check
if [ -n "$H_CCHECK" ]; then
    echo "Will try to run C style check."
    echo
    cd tools
    ./build-ccheck.sh || exit 1
    cd ..
    tools/ccheck.sh || exit 1
    echo "C style check passed."
    exit 0
fi

# Check HelenOS configuration was set-up
if [ -z "$H_ARCH" ]; then
    echo "\$H_ARCH env not set. Are you running me inside Travis?" >&2
    exit 5
fi

# Check cross-compiler target
H_CROSS_TARGET=`h_get_arch_config $H_ARCH_CONFIG_CROSS_TARGET`
if [ -z "$H_CROSS_TARGET" ]; then
    echo "No suitable cross-target found for '$H_ARCH.'" >&2
    exit 1
fi

# Default Harbours repository
if [ -z "$H_HARBOURS_REPOSITORY" ]; then
    H_HARBOURS_REPOSITORY="https://github.com/HelenOS/harbours.git"
fi

if [ "$1" = "help" ]; then
    echo
    echo "Following variables needs to be set prior running this script."
    echo "Example settings follows:"
    echo
    echo "export H_ARCH=$H_ARCH"
    echo "export TRAVIS_BUILD_ID=`date +%s`"
    echo
    echo "export H_HARBOURS=true"
    echo "export H_HARBOUR_LIST=\"$H_DEFAULT_HARBOURS_LIST\""
    echo
    echo "or"
    echo
    echo "export H_CCHECK=true"
    echo
    exit 0

elif [ "$1" = "install" ]; then
    set -x

    # Fetch and install cross-compiler
    wget "https://helenos.s3.amazonaws.com/toolchain/$H_CROSS_TARGET.tar.xz" -O "/tmp/$H_CROSS_TARGET.tar.xz" || exit 1
    sudo tar -xJ -C "/" -f "/tmp/$H_CROSS_TARGET.tar.xz" || exit 1
    exit 0

elif [ "$1" = "run" ]; then
    set -x

    # Expected output filename (bootable image)
    H_OUTPUT_FILENAME=`h_get_arch_config $H_ARCH_CONFIG_OUTPUT_FILENAME`
    if [ -z "$H_OUTPUT_FILENAME" ]; then
        echo "No suitable output image found for '$H_ARCH.'" >&2
        exit 1
    fi

    # Build HARBOURs too?
    H_HARBOURS=`echo "$H_HARBOURS" | grep -e '^true$' -e '^false$'`
    if [ -z "$H_HARBOURS" ]; then
        H_HARBOURS=false
    fi
    if [ -z "$H_HARBOUR_LIST" ]; then
        H_HARBOUR_LIST="$H_DEFAULT_HARBOURS_LIST"
    fi

    # Build it
    SRCDIR="$PWD"

    mkdir -p build/$H_ARCH || exit 1
    cd build/$H_ARCH

    export PATH="/usr/local/cross/bin:$PATH"

    $SRCDIR/configure.sh $H_ARCH || exit 1
    ninja || exit 1
    ninja image_path || exit 1

    cd $SRCDIR

    echo
    echo "HelenOS for $H_ARCH built okay."
    echo

    # Build harbours
    if $H_HARBOURS; then
        echo
        echo "Will try to build ported software for $H_ARCH."
        echo "Repository used is $H_HARBOURS_REPOSITORY."
        echo

        H_HELENOS_HOME=`pwd`
        cd "$HOME" || exit 1
        git clone --depth 10 "$H_HARBOURS_REPOSITORY" helenos-harbours || exit 1
        mkdir "build-harbours-$TRAVIS_BUILD_ID" || exit 1
        (
            cd "build-harbours-$TRAVIS_BUILD_ID" || exit 1
            mkdir build || exit 1
            cd build

            (
                #[ "$H_ARCH" = "mips32/malta-be" ] && H_ARCH="mips32eb/malta-be"
                echo "root = $H_HELENOS_HOME"
                echo "arch =" `echo "$H_ARCH" | cut -d/ -f 1`
                echo "machine =" `echo "$H_ARCH" | cut -d/ -f 2`
            ) >hsct.conf || exit 1

            "$HOME/helenos-harbours/hsct.sh" init "$H_HELENOS_HOME" $H_ARCH || exit 1

            # We cannot flood the output as Travis has limit of maximum output size
            # (reason is to prevent endless stacktraces going forever). But also Travis
            # kills a job that does not print anything for a while.
            #
            # So we store the full output into a file  and print single dot for each line.
            # As pipe tends to hide errors we check the success by checking that archive
            # exists.
            #
            FAILED_HARBOURS=""
            for HARBOUR in $H_HARBOUR_LIST; do
                "$HOME/helenos-harbours/hsct.sh" archive --no-deps "$HARBOUR" 2>&1 | tee "run-$HARBOUR.log" | awk '// {printf "."}'

                test -s "archives/$HARBOUR.tar.xz"
                if [ $? -eq 0 ]; then
                    tail -n 10 "run-$HARBOUR.log"
                else
                    FAILED_HARBOURS="$FAILED_HARBOURS $HARBOUR"
                    cat build/$HARBOUR/*/config.log
                    tail -n 100 "run-$HARBOUR.log"
                fi

            done

            if [ -n "$FAILED_HARBOURS" ]; then
                echo
                echo "ERROR: following packages were not built:$FAILED_HARBOURS."
                echo
                exit 1
            fi
        ) || exit 1
    fi
else
    echo "Invalid action specified." >&2
    exit 5
fi

