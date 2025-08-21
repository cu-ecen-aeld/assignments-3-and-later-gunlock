#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

# 1.a assign $OUTDIR to $1 or default (/tmp/aeld) if not provided
OUTDIR=${1:-/tmp/aeld} 

# 1.b create $OUTDIR if it does not exist. If failure, exit.
# NOTE: set -e (exit on any error) will exit if mkdir fails
if [ ! -d "$OUTDIR" ]; then
    mkdir -p "$OUTDIR"
fi

KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# FIX: The Github actions runner runs inside a docker image. The toolchain
#      base directory is different in the docker container. Also, both the docker
#      container and this local machine have added the toolchain bin path to
#      $PATH, so the toolchain tools (i.e. readelf) can be invoked without a path
#      prefix. Therefore, if local machine absolute paths are used in this script
#      for the toolchain base directory it will fail in the docker image. Given this,
#      the toolchain base directory should be determined at script runtime by evaluating
#      the output from 'command -v aarch64-none-linux-gnu-readelf'.  By extracting
#      the grandparent path and setting that to TOOLCHAIN_DIR, the TOOLCHAIN_SYSROOT
#      can be built.  This will set up the paths in this script to run properly
#      whether it runs on the local machine or in the docker container invoked by
#      the Github Actions runner

# Locate the readelf binary
READELF_PATH=$(command -v aarch64-none-linux-gnu-readelf)
if [ -z "$READELF_PATH" ]; then
    echo "aarch64-none-linux-gnu-readelf not found" >&2
    exit 1
fi

TOOLCHAIN_DIR=$(dirname "$(dirname "$READELF_PATH")")
TOOLCHAIN_SYSROOT=${TOOLCHAIN_DIR}/aarch64-none-linux-gnu/libc

# Validate the libc (sysroot) exists
if [ ! -d "$TOOLCHAIN_SYSROOT" ]; then
    echo "The toolchain sysroot directory (...libc) does not exist" >&2
    exit 1
fi

# Store the directory for this script
SCRIPT_DIR="$PWD"

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
    # see above
	#  OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

# see above
# mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "Cloning linux stable version ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}


    # COMPLETED: 1.c - Add your kernel build steps here
    
    # Step 1: Deep clean kernel build tree removing .conig file with any existing configs
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper

    # Step 2: Configure for our "virt" arm dev board we will simulate in QEMU
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig

    # Step 3: Build kernal image for booting with QEMU
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all

    # Step 4: Build kernel modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

    # Step 5: Build device tree (dtbs == device tree)
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
# Copy the the ${ARCH} kernel boot image, in this case ARM64.
# This is the compressed bootable kernel image for the compilation
# step above.
if [ -f "${OUTDIR}/Image" ]; then
    rm "${OUTDIR}/Image"
fi
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

# COMPLETED: 1.e Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
pushd ${OUTDIR}/rootfs
    echo "Creating rootfs directory tree here: ${OUTDIR}/rootfs"
    mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var 
    mkdir -p usr/bin /usr/lib /usr/sbin
    mkdir -p var/log
    mkdir -p home/conf
popd

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]; then
    # git clone https://git.busybox.net/busybox
    # FIX: Receiving "possible repo corruption on remote side..fatal protocol error - bad pack header"
    #    Try official repo first and fall back on github mirror if it fails
    git clone https://git.busybox.net/busybox --depth 1 --single-branch --branch ${BUSYBOX_VERSION} || \
        git clone https://github.com/mirror/busybox.git --depth 1 --single-branch --branch ${BUSYBOX_VERSION}

    cd busybox
    git checkout ${BUSYBOX_VERSION}

    # COMPLETED:  Configure busybox
    make clean
    make defconfig  # generate a default config file (.config)
else
    cd busybox
fi

# COMPLETED: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
pushd ${OUTDIR}/rootfs
    ${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
    ${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"
popd

# COMPLETED: Add library dependencies to rootfs

# dependency for busybox program interpreter (i.e. /lib/ld-linux-aarch64.so.1) and shared libs
BB_DEPS_PROG_INTERPRETER=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | \
    grep "program interpreter" | sed 's/.*: \([^]]*\).*/\1/')
BB_DEPS_SHARED_LIBS=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | \
    grep "Shared library" | sed 's/.*\[\(.*\)\]/\1/')

# copy libs from toolchain sysroot to rootfs for target
cp ${TOOLCHAIN_SYSROOT}/${BB_DEPS_PROG_INTERPRETER} ${OUTDIR}/rootfs/lib
for lib in ${BB_DEPS_SHARED_LIBS}; do
    cp ${TOOLCHAIN_SYSROOT}/lib64/$lib ${OUTDIR}/rootfs/lib64
done

# COMPLETED: Make device nodes

# null device: Major 1, Minor 3
# console device: Major 5, Minor 1
pushd ${OUTDIR}/rootfs
    sudo mknod -m 666 dev/null c 1 3
    sudo mknod -m 666 dev/console c 5 1
popd

# COMPLETED: Clean and build the writer utility
cd "$SCRIPT_DIR"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# COMPLETED: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp *.sh "${OUTDIR}/rootfs/home"
cp writer "${OUTDIR}/rootfs/home"
cp conf/username.txt "${OUTDIR}/rootfs/home/conf"
cp conf/assignment.txt "${OUTDIR}/rootfs/home/conf"

# COMPLETED: Chown the root directory
cd $OUTDIR/rootfs
sudo chown -R root:root *

# COMPLETED: Create initramfs.cpio.gz (ram disk for qemu)
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f "$OUTDIR/initramfs.cpio"
