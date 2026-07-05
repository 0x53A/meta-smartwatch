SUMMARY = "NXP NFC RF antenna tuning firmware for Fossil Gen 6 (hoki)"
DESCRIPTION = "NXP vendor-specific RF configuration for the PN553 NFC \
controller. Contains antenna tuning parameters extracted from the \
stock Wear OS libnfc-nxp.conf."
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Proprietary;md5=0557f9d92cf58f2ccdd50f62f8ac0b28"

SRC_URI = "file://nxp-nci-rf-config.bin"

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

do_install() {
    install -d ${D}${nonarch_base_libdir}/firmware/nxp
    install -m 0644 ${S}/nxp-nci-rf-config.bin \
        ${D}${nonarch_base_libdir}/firmware/nxp/nxp-nci-rf-config.bin
}

FILES:${PN} = "${nonarch_base_libdir}/firmware/nxp/nxp-nci-rf-config.bin"
