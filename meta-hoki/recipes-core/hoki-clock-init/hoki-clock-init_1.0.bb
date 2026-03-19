SUMMARY = "Initialize system clock from build date if RTC has a stale value"
DESCRIPTION = "Early-boot service that checks if the system clock is before \
the image build date (indicating an uninitialized or corrupted RTC) and sets \
it to the build timestamp. Also writes the RTC if writable."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://hoki-clock-init.sh \
           file://hoki-clock-init.service"

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "hoki-clock-init.service"

do_compile[noexec] = "1"

do_install() {
    # Substitute build epoch into the script
    BUILD_EPOCH=$(date -u +%s)
    sed "s/@BUILD_EPOCH@/$BUILD_EPOCH/" ${UNPACKDIR}/hoki-clock-init.sh > ${UNPACKDIR}/hoki-clock-init.sh.out

    install -d ${D}${prefix}/local/bin
    install -m 0755 ${UNPACKDIR}/hoki-clock-init.sh.out ${D}${prefix}/local/bin/hoki-clock-init

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/hoki-clock-init.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${prefix}/local/bin/hoki-clock-init"
