# ConnMan owns WiFi and DNS; USB addressing belongs to usb-moded.
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://main.conf"

do_install:append() {
    install -d ${D}${sysconfdir}/connman
    install -m 644 ${UNPACKDIR}/main.conf ${D}${sysconfdir}/connman/main.conf
}

# Android HAL owns physical bt_power; ConnMan controls virtual HCI rfkill.
SRC_URI:append:hoki = " file://0004-rfkill-respect-hoki-HAL-power-ownership.patch"
# This hardware-specific policy must not share an architecture-wide package.
PACKAGE_ARCH:hoki = "${MACHINE_ARCH}"
