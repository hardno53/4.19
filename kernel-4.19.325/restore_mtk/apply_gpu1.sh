#!/bin/bash
M=drivers/misc/mediatek
D=arch/arm64/configs/a02_64_bootstrap_defconfig
# base/power hook (gpufreq mt6739 only)
printf 'obj-y += power/\n' > $M/base/Makefile
printf 'obj-$(CONFIG_MACH_MT6739) += mt6739/\n' > $M/base/power/Makefile
# mediatek Makefile hook
grep -q '^obj-y += base/' $M/Makefile || printf 'obj-y += base/\n' >> $M/Makefile
grep -q '^obj-y += gpu/' $M/Makefile || printf 'obj-y += gpu/\n' >> $M/Makefile
# Kconfig hook
grep -q 'gpu/Kconfig' $M/Kconfig || sed -i 's|^source "drivers/misc/mediatek/leds/Kconfig"|&\nsource "drivers/misc/mediatek/gpu/Kconfig"|' $M/Kconfig
# defconfig
for l in 'CONFIG_MTK_GPU_SUPPORT=y' 'CONFIG_MTK_GPU_VERSION="rgx auckland 1.8ED"' 'CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT=y' 'CONFIG_SYNC_FILE=y' 'CONFIG_DEBUG_FS=y'; do
  k=${l%%=*}; grep -q "^$k=" $D || { sed -i "/^# $k is not set/d" $D; echo "$l" >> $D; }
done
echo "apply_gpu1 OK"; grep -n 'GPU\|SYNC_FILE\|DEBUG_FS' $D | head
