#!/bin/bash
set -e
R=restore_mtk
M=drivers/misc/mediatek
cp $R/curated/Makefile $M/Makefile
cp $R/curated/Kconfig $M/Kconfig
cp -r $R/files/. .
bash $R/apply_gpu1.sh
bash $R/apply_gpu2.sh
mkdir -p $M/shim
[ -f $M/shim/Makefile ] || echo '# shim stub dir (isi lama hilang)' > $M/shim/Makefile
SPI=/workspaces/ref_oc/include/linux/platform_data/spi-mt65xx.h
[ -f $SPI ] && cp $SPI include/linux/platform_data/spi-mt65xx.h
echo "restore_mtk OK"
