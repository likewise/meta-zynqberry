FILESEXTRAPATHS_prepend := "${THISDIR}:"

SRC_URI += "file://zynqberry.dts \
file://defconfig"

do_copy_devicetree () {
#	mkdir -p ${S}/arch/arm/dts
    cp ${WORKDIR}/zynqberry.dts ${S}/arch/arm/boot/dts/
}

addtask copy_devicetree after do_patch before do_configure
