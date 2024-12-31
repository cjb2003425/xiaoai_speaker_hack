#!/bin/sh

. /lib/functions/system.sh
. /lib/meson.sh

do_meson() {
    meson_board_detect
}

boot_hook_add preinit_main do_meson
