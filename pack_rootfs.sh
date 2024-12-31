#!/usr/bin/bash

mksquashfs rootfs rootfs.img -b 131072 -comp xz -no-xattrs
