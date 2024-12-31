#!/bin/sh
#

klogger(){
	local msg1="$1"
	local msg2="$2"

	if [ "$msg1" = "-n" ]; then
		echo  -n "$msg2" >> /dev/kmsg 2>/dev/null
	else
		echo "$msg1" >> /dev/kmsg 2>/dev/null
	fi

	return 0
}

stop_service(){
	echo 3 > /proc/sys/vm/drop_caches
}

restore_service(){
	return
}


# test if $1 has $2 inside
bingo() {
	miso -c $1 -f $2 > /dev/null
	return $?
}


pipe_upgrade() {
	local package=$1
	local segment_name=$2
	local dest=$3
	local ret=1

	miso -c $package -f $segment_name > /dev/null
	if [ $? -eq 0 ]; then
		klogger -n "Burning $segment_name to $mtd_dev ..."

		exec 9>&1
		local pipestatus0=`((miso -r -x $package -f $segment_name -n || echo $? >&8) | \
			mtd write - $dest) 8>&1 >&9`
		if [ -z "$pipestatus0" -a $? -eq 0 ]; then
			ret=0
		else
			ret=1
		fi
		exec 9>&-
	fi

	return $ret
}


updtb() {
    bingo $1 "dtb.img"
    if [ $? != 0 ]; then
        bingo $1 "meson.dtb"
        if [ $? != 0 ]; then
            return 0
        else
            target="meson.dtb"
        fi
    else
        target="dtb.img"
    fi

	klogger "Updating dtb..."

	miso -r -x $1 -f $target
	dd if=$target of=/dev/dtb bs=128K count=1
	rm -f $target

	return 0
}


upboot() {
	bingo $1 u-boot.bin.usb.bl2 || return 0
	bingo $1 u-boot.bin.usb.tpl || return 0

	if ! type nandwrite>/dev/null 2>&1; then
	        return 0
	fi

	miso -r -x $1 -f u-boot.bin.usb.bl2
	miso -r -x $1 -f u-boot.bin.usb.tpl

	klogger "Burning uboot..."
	uboot_mtd=$(grep bootloader /proc/mtd | awk -F: '{print substr($1,4)}')
	tpl_mtd=$(grep tpl /proc/mtd | awk -F: '{print substr($1,4)}')

	#bl2:1
	flash_erase -N /dev/mtd$uboot_mtd 0 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		klogger "Burning bl2 error"
		return 1
	fi
	
	#tpl:1
	flash_erase -N /dev/mtd$tpl_mtd 0 16 > /dev/null 2>&1
	nandwrite /dev/mtd$tpl_mtd u-boot.bin.usb.tpl -p > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		klogger "Burning tpl error"
		return 1
	fi
	
	#bl2:2
	flash_erase -N /dev/mtd$uboot_mtd 0x40000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0x40000 > /dev/null 2>&1
	
	#bl2:3
	flash_erase -N /dev/mtd$uboot_mtd 0x80000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0x80000 > /dev/null 2>&1
	
	#bl2:4
	flash_erase -N /dev/mtd$uboot_mtd 0xc0000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0xc0000 > /dev/null 2>&1
	
	#bl2:5
	flash_erase -N /dev/mtd$uboot_mtd 0x100000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0x100000 > /dev/null 2>&1
	
	#bl2:6
	flash_erase -N /dev/mtd$uboot_mtd 0x140000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0x14000 > /dev/null 2>&1
	
	#bl2:7
	flash_erase -N /dev/mtd$uboot_mtd 0x180000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0x180000 > /dev/null 2>&1
	
	#bl2:8
	flash_erase -N /dev/mtd$uboot_mtd 0x1c0000 2 > /dev/null 2>&1
	nandwrite /dev/mtd$uboot_mtd u-boot.bin.usb.bl2  --start=0x1c0000 > /dev/null 2>&1

	#tpl:2
	flash_erase -N /dev/mtd$tpl_mtd 2097152  16 > /dev/null 2>&1
	nandwrite /dev/mtd$tpl_mtd u-boot.bin.usb.tpl --start=0x200000 -p > /dev/null 2>&1
	
	#tpl:3
	flash_erase -N /dev/mtd$tpl_mtd 4194304  16 > /dev/null 2>&1
	nandwrite /dev/mtd$tpl_mtd u-boot.bin.usb.tpl  --start=0x400000 -p > /dev/null 2>&1
	
	#tpl:4
	flash_erase -N /dev/mtd$tpl_mtd 6291456  16 > /dev/null 2>&1
	nandwrite /dev/mtd$tpl_mtd u-boot.bin.usb.tpl  --start=0x600000 -p > /dev/null 2>&1

	rm -f u-boot.bin.usb.bl2
	rm -f u-boot.bin.usb.tpl

	return 0
}

upker() {
	local target="boot.img"
	local dev="/dev/mtd"$kernel_mtd_target""

	bingo $1 $target || return 1

	klogger "Burning $dev kernel Block"
	echo "Burning $kernel_mtd_target kernel"

	pipe_upgrade $1 $target $dev
	if [ $? -eq 0 ]; then
		klogger "Done"
		return 0
	fi

	# kernel upgrade failed. failure
	restore_service
	return 1
}

upfs_squash() {
	local target="root.squashfs"
	local dev="/dev/mtd"$rootfs_mtd_target""

	bingo $1 $target || return 1

	klogger "Burning $dev rootfs Block"
	echo "Burning $rootfs_mtd_target rootfs"


	pipe_upgrade $1 $target $dev
	if [ $? -eq 0 ]; then
 		klogger "Done"
 		return 0
	fi
 
	# rootfs upgrade failed. failure
	return 1
}

board_upgrade_param_check() {

	return 0
}

board_upgrade_done_set_flag() {
	#upgrade success. set flags
	kernel_mtd_current=`fw_env -g boot_part`

	if [ "$kernel_mtd_current" = "boot0" ]; then
	    /usr/bin/fw_env -s boot_part boot1
	else
	    /usr/bin/fw_env -s boot_part boot0
	fi

	return 0
}

board_prepare_upgrade() {
	stop_service
}

board_start_upgrade_led() {
	return
}

board_system_upgrade() {
	local filename=$1
	uboot_mtd=$(grep bootloader /proc/mtd | awk -F: '{print substr($1,4)}')

	kernel0_mtd=$(grep boot0 /proc/mtd | awk -F: '{print substr($1,4)}')
	kernel1_mtd=$(grep boot1 /proc/mtd | awk -F: '{print substr($1,4)}')

	rootfs0_mtd=$(grep system0 /proc/mtd | awk -F: '{print substr($1,4)}')
	rootfs1_mtd=$(grep system1 /proc/mtd | awk -F: '{print substr($1,4)}')

	kernel_mtd_current=`fw_env -g boot_part`

	if [ "$kernel_mtd_current" = "boot0" ]; then
		kernel_mtd_target=$kernel1_mtd
		rootfs_mtd_target=$rootfs1_mtd
		klogger "updating part 1"
	elif [ "$kernel_mtd_current" = "boot1" ]; then
		kernel_mtd_target=$kernel0_mtd
		rootfs_mtd_target=$rootfs0_mtd
		klogger "updating part 0"
	else
		klogger "error boot env: can not find boot_part."
		return 1
	fi


	# Version file exist?
	bingo $filename "mico_version" && {
		miso -r -x $filename -f "mico_version"
		klogger "updating to `cat mico_version | grep "option ROM" | awk '{ print $3 }'`..."
	}

	updtb $filename || return 1
	upboot $filename || return 1
	upker $filename || return 1
	upfs_squash $filename || return 1

	echo "burn done"

	return 0
}
