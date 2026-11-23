#!/bin/bash
M=drivers/misc/mediatek
G=$M/gpu
# 1. picachu butuh mtk_eem.h (belum ada), gpufreq gak pakai -> drop
sed -i '/mtk_picachu/d' $M/base/power/mt6739/Makefile
# 2. ged_main: of_device_id
grep -q 'linux/of_device.h' $G/ged/src/ged_main.c || sed -i '0,/#include <linux\/cdev.h>/s||#include <linux/cdev.h>\n#include <linux/of.h>\n#include <linux/of_device.h>|' $G/ged/src/ged_main.c
# 3. pvr_ion_stats: struct ion_buffer lengkap
F=$G/gpu_rgx/m1.13ED5776728/services/server/env/linux/pvr_ion_stats.c
grep -q '"ion_priv.h"' $F || sed -i '0,/#include PVR_ANDROID_ION_PRIV_HEADER/s||#include PVR_ANDROID_ION_PRIV_HEADER\n#include "ion_priv.h"|' $F
# 4. stub mtk_events.h
echo "apply_gpu2 OK"
grep -n 'of_device\|ion_priv' $G/ged/src/ged_main.c $F | head
