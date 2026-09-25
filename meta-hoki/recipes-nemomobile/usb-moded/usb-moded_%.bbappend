FILESEXTRAPATHS:prepend := "${THISDIR}/usb-moded:"
SRC_URI += "file://0005-usb-moded-configfs-Add-ADB-function-support.patch"
SRC_URI += " file://0008-udev-transient-receive.patch"
SRC_URI += " file://0006-configfs-wait-for-adb-descriptors.patch file://0007-network-idempotent-address.patch file://usb-moded.ini file://adb_mode.ini file://developer_mode.ini file://adbd-functionfs.sh"

do_install:append() {
    install -m 0644 ${UNPACKDIR}/usb-moded.ini ${D}${sysconfdir}/usb-moded/usb-moded.ini
    install -m 0644 ${UNPACKDIR}/adb_mode.ini ${D}${sysconfdir}/usb-moded/dyn-modes/adb_mode.ini
    install -m 0644 ${UNPACKDIR}/developer_mode.ini ${D}${sysconfdir}/usb-moded/dyn-modes/developer_mode.ini
    install -m 0755 ${UNPACKDIR}/adbd-functionfs.sh ${D}${sbindir}/adbd-functionfs.sh
    # ADB mode includes NCM; SSH mode exposes NCM without ADB.
    for mode in adb_mode developer_mode; do
        for spec in 'prepare adbd-prepare.service 0' 'start android-tools-adbd.service 0' 'dhcp udhcp-daemon.service 1'; do
            set -- $spec
            [ "$mode" = developer_mode ] && [ "$1" != dhcp ] && continue
            printf '[info]\nname=%s\nmode=%s\nsystemd=1\npost=%s\n' "$2" "$mode" "$3" > ${D}${sysconfdir}/usb-moded/run/hoki-$mode-$1.ini
        done
    done
    rm -f ${D}${sysconfdir}/usb-moded/run/adb-prepare.ini ${D}${sysconfdir}/usb-moded/run/adb-startserver.ini ${D}${sysconfdir}/usb-moded/run/udhcpd-developer-mode.ini
}
