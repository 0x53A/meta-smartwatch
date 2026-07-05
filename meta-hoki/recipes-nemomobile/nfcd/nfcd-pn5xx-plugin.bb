SUMMARY = "nfcd plugin for NXP PN5xx NFC controllers via direct NCI"
DESCRIPTION = "Talks directly to /dev/nq-nci (PN553 kernel driver) \
without going through the Android NFC HAL. Handles power management \
and injects NXP RF antenna tuning config after NCI CORE_INIT."
HOMEPAGE = "https://github.com/AsteroidOS"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://src/pn5xx_plugin.c;beginline=1;endline=10;md5=dccf7ffc1246b882f6e8097b5236218f"

FILESEXTRAPATHS:prepend := "${THISDIR}/nfcd-pn5xx-plugin:"
SRC_URI = "file://src \
           file://Makefile \
           file://nxp-rf-config.bin \
           "

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

DEPENDS = "glib-2.0 libglibutil libncicore libnciplugin nfcd"

inherit pkgconfig

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${libdir}/nfcd/plugins
    install -m 0644 ${S}/pn5xx.so ${D}${libdir}/nfcd/plugins/

    install -d ${D}${sysconfdir}/nfc
    install -m 0644 ${S}/nxp-rf-config.bin ${D}${sysconfdir}/nfc/
}

FILES:${PN} = "${libdir}/nfcd/plugins/pn5xx.so ${sysconfdir}/nfc/nxp-rf-config.bin"
