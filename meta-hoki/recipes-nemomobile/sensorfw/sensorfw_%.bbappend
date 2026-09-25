FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://0007-Add-SpO2-sensor-with-hybris-adaptor.patch \
            file://0008-Add-vendor-health-sensors.patch \
            file://0009-Fix-compass-degrees-use-qreal-and-fmod-instead-of-int.patch"

SRC_URI:append:hoki = " file://hoki-start-sensors file://hoki-sensors.conf"
RDEPENDS:${PN}:append:hoki = " libhybris"

do_install:append:hoki() {
    install -Dm0755 ${UNPACKDIR}/hoki-start-sensors ${D}${sbindir}/hoki-start-sensors
    install -Dm0644 ${UNPACKDIR}/hoki-sensors.conf ${D}${systemd_system_unitdir}/sensorfwd.service.d/hoki-sensors.conf
}

FILES:${PN}:append:hoki = " ${sbindir}/hoki-start-sensors ${systemd_system_unitdir}/sensorfwd.service.d/hoki-sensors.conf"
