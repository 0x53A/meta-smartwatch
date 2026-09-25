FILESEXTRAPATHS:prepend := "${THISDIR}/android-tools:"
SRC_URI += "file://hoki-adbd.conf"
do_install:append() {
    install -Dm0644 ${UNPACKDIR}/hoki-adbd.conf ${D}${systemd_system_unitdir}/android-tools-adbd.service.d/hoki.conf
}
FILES:${PN}-adbd += "${systemd_system_unitdir}/android-tools-adbd.service.d"
