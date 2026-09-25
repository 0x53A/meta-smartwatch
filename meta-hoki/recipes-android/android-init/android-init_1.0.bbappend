FILESEXTRAPATHS:prepend:hoki := "${THISDIR}/${PN}:"

SRC_URI:append:hoki = " file://nonplat_property_contexts \
    file://plat_property_contexts \
    file://hoki-stop-bluetooth file://hoki-stop-bluetooth.conf"

do_install:append:hoki() {
    install -m 0644 ${UNPACKDIR}/nonplat* ${D}/
    install -m 0644 ${UNPACKDIR}/plat* ${D}/
    install -Dm0755 ${UNPACKDIR}/hoki-stop-bluetooth ${D}${sbindir}/hoki-stop-bluetooth
    install -Dm0644 ${UNPACKDIR}/hoki-stop-bluetooth.conf ${D}${systemd_system_unitdir}/android-init.service.d/hoki-stop-bluetooth.conf
}

FILES:${PN}:append:hoki = " /nonplat* /plat* ${sbindir}/hoki-stop-bluetooth"

# Supplies getprop/setprop used while Android init still owns its HAL children.
RDEPENDS:${PN}:append:hoki = " libhybris"
