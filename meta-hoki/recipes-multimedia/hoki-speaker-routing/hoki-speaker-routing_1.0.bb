SUMMARY = "Enable the Hoki built-in speaker and microphone routes"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
COMPATIBLE_MACHINE = "^hoki$"
PACKAGE_ARCH = "${MACHINE_ARCH}"

SRC_URI = "file://hoki-speaker-routing.service file://90-hoki-speaker-routing.rules"
S = "${UNPACKDIR}"

inherit systemd

RDEPENDS:${PN} = "tinyalsa"
SYSTEMD_SERVICE:${PN} = "hoki-speaker-routing.service"
# The card ID may not be populated during controlC0's add event. Also enable
# through sound.target; the service waits for the actual ALSA control device.
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    install -Dm0644 ${UNPACKDIR}/hoki-speaker-routing.service ${D}${systemd_system_unitdir}/hoki-speaker-routing.service
    sed -i 's|@BINDIR@|${bindir}|g' ${D}${systemd_system_unitdir}/hoki-speaker-routing.service
    install -Dm0644 ${UNPACKDIR}/90-hoki-speaker-routing.rules ${D}${nonarch_base_libdir}/udev/rules.d/90-hoki-speaker-routing.rules
}

FILES:${PN} += "${nonarch_base_libdir}/udev/rules.d/90-hoki-speaker-routing.rules"
