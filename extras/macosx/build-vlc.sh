#!/bin/sh
# Copyright (C) Pierre d'Herbemont, 2010
# Copyright (C) Felix Paul Kühne, 2012-2018

set -e

info()
{
    local green="\033[1;32m"
    local normal="\033[0m"
    echo "[${green}build${normal}] $1"
}

OSX_VERSION=`xcrun --sdk macosx --show-sdk-version`
ARCH="x86_64"
MINIMAL_OSX_VERSION="10.7"
SDKROOT=`xcode-select -print-path`/Platforms/MacOSX.platform/Developer/SDKs/MacOSX$OSX_VERSION.sdk
UNSTABLE=no

if [ -z "$MAKE_JOBS" ]; then
    CORE_COUNT=`sysctl -n machdep.cpu.core_count`
    let MAKE_JOBS=$CORE_COUNT+1
fi

usage()
{
cat << EOF
usage: $0 [-v] [-d]

OPTIONS
   -v            Be more verbose
   -u            Use unstable libvlc
   -k <sdk>      Use the specified sdk (default: $SDKROOT for $ARCH)
   -a <arch>     Use the specified arch (default: $ARCH)
EOF
}

spushd()
{
     pushd "$1" 2>&1> /dev/null
}

spopd()
{
     popd 2>&1> /dev/null
}

info()
{
     local green="\033[1;32m"
     local normal="\033[0m"
     echo "[${green}info${normal}] $1"
}

while getopts "hvua:k:" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
             ;;
         v)
             VERBOSE=yes
             ;;
         u)
             UNSTABLE=yes
             ;;
         a)
             ARCH=$OPTARG
             ;;
         k)
             SDKROOT=$OPTARG
             ;;
         ?)
             usage
             exit 1
             ;;
     esac
done
shift $(($OPTIND - 1))

out="/dev/null"
if [ "$VERBOSE" = "yes" ]; then
   out="/dev/stdout"
fi

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

export OSX_VERSION

# Get root dir
spushd .
npapi_root_dir=`pwd`
spopd

info $npapi_root_dir

info "Preparing build dirs"

spushd extras/macosx

if ! [ -e vlc ]; then
if [ "$UNSTABLE" = "yes" ]; then
git clone git://git.videolan.org/vlc.git vlc
else
git clone git://git.videolan.org/vlc/vlc-3.0.git vlc
fi
fi

spopd #extras/macosx

#
# Build time
#

export PATH="${npapi_root_dir}/extras/macosx/vlc/extras/tools/build/bin:${npapi_root_dir}/extras/macosx/contrib/${ARCH}-apple-darwin14/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin"

info "Building tools"
spushd extras/macosx/vlc/extras/tools
if ! [ -e build ]; then
./bootstrap && make
fi
spopd

info "Exporting environment"

if [ ! -d "${SDKROOT}" ]
then
    echo "*** ${SDKROOT} does not exist, please install required SDK, or set SDKROOT manually. ***"
    exit 1
fi

# partially clean the environment
export CFLAGS=""
export CPPFLAGS=""
export CXXFLAGS=""
export OBJCFLAGS=""
export LDFLAGS=""

export PLATFORM=$PLATFORM
export SDK_VERSION=$SDK_VERSION
export VLCSDKROOT=$SDKROOT

CFLAGS="-isysroot ${SDKROOT} -arch ${ARCH} ${OPTIM}"
OBJCFLAGS="${OPTIM}"

CFLAGS+=" -mmacosx-version-min=${MINIMAL_OSX_VERSION}"

export CFLAGS="${CFLAGS}"
export CXXFLAGS="${CFLAGS}"
export CPPFLAGS="${CFLAGS}"
export OBJCFLAGS="${OBJCFLAGS}"

export LDFLAGS="-arch ${ARCH}"

EXTRA_CFLAGS="-arch ${ARCH}"
EXTRA_CFLAGS+=" -mmacosx-version-min=${MINIMAL_OSX_VERSION}"
EXTRA_LDFLAGS=" -Wl,-macosx_version_min,${MINIMAL_OSX_VERSION}"
export LDFLAGS="${LDFLAGS} -v -Wl,-macosx_version_min,${MINIMAL_OSX_VERSION}"

# The following symbols do not exist on the minimal macOS version (10.7), so they are disabled
# here. This allows compilation also with newer macOS SDKs.
# Added symbols in 10.13
export ac_cv_func_open_wmemstream=no
export ac_cv_func_fmemopen=no
export ac_cv_func_open_memstream=no
export ac_cv_func_futimens=no
export ac_cv_func_utimensat=no

# Added symbols between 10.11 and 10.12
export ac_cv_func_basename_r=no
export ac_cv_func_clock_getres=no
export ac_cv_func_clock_gettime=no
export ac_cv_func_clock_settime=no
export ac_cv_func_dirname_r=no
export ac_cv_func_getentropy=no
export ac_cv_func_mkostemp=no
export ac_cv_func_mkostemps=no

# Added symbols between 10.7 and 10.11
export ac_cv_func_ffsll=no
export ac_cv_func_flsll=no
export ac_cv_func_fdopendir=no
export ac_cv_func_openat=no
export ac_cv_func_fstatat=no
export ac_cv_func_readlinkat=no

export CC="xcrun clang"
export CXX="xcrun clang++"
export OBJC="xcrun clang"

info "Fetching contrib"

spushd extras/macosx/vlc/contrib

if ! [ -e ${ARCH}-npapi ]; then
mkdir ${ARCH}-npapi
fi
cd ${ARCH}-npapi

export USE_FFMPEG=1
../bootstrap --build=${ARCH}-apple-darwin14 \
    --enable-ad-clauses \
    --disable-disc \
    --disable-sdl \
    --disable-SDL_image \
    --disable-iconv \
    --enable-zvbi \
    --disable-kate \
    --disable-caca \
    --disable-gettext \
    --disable-mpcdec \
    --disable-upnp \
    --disable-gme \
    --disable-srt \
    --disable-tremor \
    --enable-vorbis \
    --disable-sidplay2 \
    --disable-samplerate \
    --disable-goom \
    --disable-vncserver \
    --disable-orc \
    --disable-schroedinger \
    --disable-libmpeg2 \
    --disable-chromaprint \
    --disable-mad \
    --enable-fribidi \
    --enable-libxml2 \
    --enable-freetype2 \
    --enable-ass \
    --disable-fontconfig \
    --disable-gpg-error \
    --disable-vncclient \
    --disable-gnutls \
    --disable-lua \
    --disable-luac \
    --disable-aribb24 \
    --disable-aribb25 \
    --enable-libdsm \
    --enable-libplacebo \
    --disable-sparkle \
    --disable-growl \
    --disable-breakpad \
    --disable-ncurses \
    --disable-asdcplib \
    --enable-soxr \
    --disable-protobuf \
    --disable-sout \
    --disable-fontconfig \
    --disable-bghudappkit \
    --disable-twolame \
    --disable-microdns \
    --disable-SDL \
    --disable-SDL_image \
    --disable-cddb \
    --disable-bluray \
    --disable-vncserver \
    --disable-vpx

echo "EXTRA_CFLAGS += ${EXTRA_CFLAGS}" >> config.mak
echo "EXTRA_LDFLAGS += ${EXTRA_LDFLAGS}" >> config.mak

make fetch -j$MAKE_JOBS
make -j$MAKE_JOBS > ${out}

spopd

PREFIX="${npapi_root_dir}/extras/macosx/vlc/${ARCH}-install"

info "Configuring VLC"

if ! [ -e ${PREFIX} ]; then
    mkdir ${PREFIX}
fi

spushd extras/macosx/vlc
if ! [ -e configure ]; then
    ./bootstrap > ${out}
fi
if ! [ -e ${ARCH}-build ]; then
    mkdir ${ARCH}-build
fi

# Available but not authorized
export ac_cv_func_daemon=no
export ac_cv_func_fork=no

export CXXFLAGS="${CXXFLAGS} -stdlib=libc++"
export LDFLAGS="${LDFLAGS} -read_only_relocs suppress"

cd ${ARCH}-build
../configure \
        --build=${ARCH}-apple-darwin14 \
        --prefix=${PREFIX} \
        --with-macosx-version-min=${MINIMAL_OSX_VERSION} \
        --with-macosx-sdk=$SDKROOT \
        --disable-lua --disable-vlm --disable-sout \
        --disable-vcd --disable-screen \
        --disable-debug \
        --disable-macosx \
        --disable-notify \
        --disable-projectm \
        --disable-faad \
        --disable-bluray \
        --enable-flac \
        --enable-theora \
        --enable-shout \
        --disable-ncurses \
        --disable-twolame \
        --enable-realrtsp \
        --enable-libass \
        --disable-macosx-avfoundation \
        --disable-macosx-qtkit \
        --disable-skins2 \
        --disable-xcb \
        --disable-caca \
        --disable-samplerate \
        --disable-upnp \
        --disable-goom \
        --disable-nls \
        --disable-sdl-image \
        --disable-sparkle \
        --disable-addonmanagermodules \
        --disable-mad \
        --enable-merge-ffmpeg \
        > ${out}

info "Compiling VLC"

if [ "$VERBOSE" = "yes" ]; then
    make V=1 -j$MAKE_JOBS > ${out}
else
    make -j$MAKE_JOBS > ${out}
fi

info "Installing VLC"
make install > ${out}
cd ..

info "Removing unneeded modules"
blacklist="
stats
access_bd
shm
access_imem
oldrc
real
hotkeys
gestures
sap
dynamicoverlay
rss
ball
magnify
audiobargraph_
clone
mosaic
osdmenu
puzzle
mediadirs
t140
ripple
motion
sharpen
grain
posterize
mirror
wall
scene
blendbench
psychedelic
alphamask
netsync
audioscrobbler
motiondetect
motionblur
export
smf
podcast
bluescreen
erase
stream_filter_record
speex_resampler
remoteosd
magnify
gradient
logger
visual
fb
aout_file
dummy
invert
sepia
wave
hqdn3d
headphone_channel_mixer
gaussianblur
gradfun
extract
colorthres
antiflicker
anaglyph
remap
"

for i in ${blacklist}
do
    find ${PREFIX}/lib/vlc/plugins -name *$i* -type f -exec rm '{}' \;
done

spopd

info "Build completed"
