SUMMARY = "Vendor firmware links for the hoki Prima WLAN driver"
DESCRIPTION = "Expose the existing Android vendor/persist WLAN configuration and calibration files at the paths requested by Prima."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
COMPATIBLE_MACHINE = "hoki"
PACKAGE_ARCH = "${MACHINE_ARCH}"
PR = "r1"

S = "${UNPACKDIR}"

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    # Package the links, not the device's proprietary firmware/calibration data.
    # On usrmerge images /lib resolves to /usr/lib; use the distro's location.
    install -d ${D}${nonarch_base_libdir}/firmware/wlan/prima
    ln -s /vendor/etc/wifi/WCNSS_qcom_cfg.ini ${D}${nonarch_base_libdir}/firmware/wlan/prima/WCNSS_qcom_cfg.ini
    ln -s /mnt/vendor/persist/WCNSS_qcom_wlan_nv.bin ${D}${nonarch_base_libdir}/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin
    ln -s /mnt/vendor/persist/WCNSS_wlan_dictionary.dat ${D}${nonarch_base_libdir}/firmware/wlan/prima/WCNSS_wlan_dictionary.dat
}

FILES:${PN} = "${nonarch_base_libdir}/firmware/wlan/prima"
