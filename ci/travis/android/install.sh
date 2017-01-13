#!/bin/sh

set -e

wget -c -N -P $HOME/downloads https://dl.google.com/android/repository/android-ndk-r13b-linux-x86_64.zip
unzip -q $HOME/downloads/android-ndk-r13b-linux-x86_64.zip -d $HOME/android-ndk
$HOME/android-ndk/android-ndk-r13b/build/tools/make-standalone-toolchain.sh --arch=arm --platform=android-8 --install-dir=$HOME/android-toolchain
