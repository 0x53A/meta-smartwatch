FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append:hoki = " file://gps_xtra.ini"
PR:append:hoki = ".hoki1"

do_install:append:hoki() {
    install -Dm0644 ${UNPACKDIR}/gps_xtra.ini ${D}${sysconfdir}/gps_xtra.ini
}

FILES:${PN}:append:hoki = " ${sysconfdir}/gps_xtra.ini"
CONFFILES:${PN}:append:hoki = " ${sysconfdir}/gps_xtra.ini"
