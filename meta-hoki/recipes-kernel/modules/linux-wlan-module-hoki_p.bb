SUMMARY = "Prima WLAN driver for hoki"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://CORE/HDD/src/wlan_hdd_main.c;beginline=1;endline=26;md5=369b35dc0fad1adbdb95e7f88afdecde"
COMPATIBLE_MACHINE = "hoki"

inherit module kernel-module-split systemd

SRC_URI = "git://github.com/Dev-msm8953/vendor_qcom_opensource_wlan_prima.git;branch=LA.UM.10.6.2.r1-02500-89xx.0;protocol=https \
           file://0001-Add-wakelock.h-stub.patch \
           file://0002-Fix-NULL-adapter-deref-in-hdd_process_bt_sco_profile.patch \
           file://wlan-module-load.service"
SRCREV = "3401b532668f7dab145d1fc4f5c3f0ab99abd663"
PV = "1.0+pie"
S = "${WORKDIR}/git"
B = "${S}"

DEPENDS = "virtual/kernel python3-native"

# The kernel's gcc-wrapper.py is python2 — bypass it via CC.
# Add our wakelock stub to the include path.
do_compile() {
    oe_runmake -C ${STAGING_KERNEL_DIR} \
        M=${S} \
        MODNAME=wlan \
        WLAN_ROOT=${S} \
        CONFIG_PRONTO_WLAN=m \
        BOARD_PLATFORM=msm8937 \
        CC="${KERNEL_CC}" \
        KCFLAGS="-I${S}/wakelock_stub" \
        modules
}

# Module is loaded by wlan-module-load.service (needs WCNSS firmware init first)
KERNEL_MODULE_AUTOLOAD = ""
KERNEL_MODULE_PROBECONF = ""

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "wlan-module-load.service"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 644 ${UNPACKDIR}/wlan-module-load.service ${D}${systemd_system_unitdir}/wlan-module-load.service
}

RPROVIDES:${PN} = "kernel-module-wlan"

# Set up firmware symlinks and MAC address file at install time.
# The Prima driver expects firmware at /lib/firmware/wlan/prima/ but the
# actual files live on the Android vendor/persist partitions.
pkg_postinst:${PN}() {
    mkdir -p $D/lib/firmware/wlan/prima
    ln -sf /vendor/etc/wifi/WCNSS_qcom_cfg.ini $D/lib/firmware/wlan/prima/WCNSS_qcom_cfg.ini
    ln -sf /mnt/vendor/persist/WCNSS_qcom_wlan_nv.bin $D/lib/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin
    ln -sf /mnt/vendor/persist/WCNSS_wlan_dictionary.dat $D/lib/firmware/wlan/prima/WCNSS_wlan_dictionary.dat
    # Generate a locally-administered random MAC if none exists
    if [ -z "$D" ] && [ ! -f /lib/firmware/wlan/prima/wlan_mac.bin ]; then
        printf '\x02' > /lib/firmware/wlan/prima/wlan_mac.bin
        dd if=/dev/urandom bs=1 count=23 >> /lib/firmware/wlan/prima/wlan_mac.bin 2>/dev/null
    fi
}
