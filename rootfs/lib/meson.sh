#!/bin/sh
#
# Copyright (C) 2009-2011 OpenWrt.org
#

MESON_BOARD_NAME=

meson_get_mtd_offset_size_format() {
	local mtd="$1"
	local offset="$2"
	local size="$3"
	local format="$4"
	local dev

	dev=$(find_mtd_part $mtd)
	[ -z "$dev" ] && return

	dd if=$dev bs=1 skip=$offset count=$size 2>/dev/null | hexdump -v -e "1/1 \"$format\""
}

cybertan_get_hw_magic() {
	local part

	part=$(find_mtd_part firmware)
	[ -z "$part" ] && return 1

	dd bs=8 count=1 skip=0 if=$part 2>/dev/null | hexdump -v -n 8 -e '1/1 "%02x"'
}

meson_board_detect() {
	local machine
	local name

	machine=$(awk 'BEGIN{FS="[ \t]+:[ \t]"} /Hardware/ {print $2}' /proc/cpuinfo)

	case "$machine" in
	*"Amlogic Meson3")
		name="f16"
		;;
	esac

	[ -z "$name" ] && name="unknown"

	[ -z "$MESON_BOARD_NAME" ] && MESON_BOARD_NAME="$name"

	[ -e "/tmp/sysinfo/" ] || mkdir -p "/tmp/sysinfo/"

	echo "$MESON_BOARD_NAME" > /tmp/sysinfo/board_name
}

meson_board_name() {
	local name

	[ -f /tmp/sysinfo/board_name ] && name=$(cat /tmp/sysinfo/board_name)
	[ -z "$name" ] && name="unknown"

	echo "$name"
}
