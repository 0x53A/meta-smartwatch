FILESEXTRAPATHS:prepend:hoki := "${THISDIR}/${PN}:"

SRC_URI:append:hoki = " file://nfc-power-off.service"

do_install:append:hoki() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/nfc-power-off.service ${D}${systemd_system_unitdir}/
}

pkg_postinst:${PN}:hoki() {
    if [ -n "$D" ]; then
        mkdir -p $D${sysconfdir}/systemd/system/multi-user.target.wants
        ln -sf ${systemd_system_unitdir}/nfc-power-off.service $D${sysconfdir}/systemd/system/multi-user.target.wants/
    fi
}

FILES:${PN}:append:hoki = " ${systemd_system_unitdir}/nfc-power-off.service"
