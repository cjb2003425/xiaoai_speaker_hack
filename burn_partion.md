# Burn partion:

## Get current boot partion
```
fw_env -g boot_part
boot0
```

## Write the image to another partition
```
mtd write /tmp/rootfs.img system1
Unlocking system1 ...

Writing from /tmp/rootfs.img to system1 ...
```

## Set the boot partition to the new one
```
fw_env -s boot_part boot1
[ubootenv] update_bootenv_varible name [boot_part]: value [boot1]
[ubootenv] Save ubootenv to /dev/nand_env succeed!
```

### Reboot
