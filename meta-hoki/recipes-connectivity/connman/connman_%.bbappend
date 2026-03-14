# Remove ConnMan's tmpfiles rule that symlinks /etc/resolv.conf to
# /run/connman/resolv.conf. This conflicts with systemd-resolved's own
# rule (which symlinks to the stub resolver) and takes priority because
# /etc/tmpfiles.d/ overrides /usr/lib/tmpfiles.d/. The result is broken
# DNS when systemd-resolved is the active resolver.
#
# Also install main.conf to blacklist the p2p0 interface created by
# the Prima WLAN driver — without this, ConnMan tries to associate
# on p2p0 instead of wlan0.
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://main.conf"

do_install:append() {
    rm -f ${D}${sysconfdir}/tmpfiles.d/connman_resolvconf.conf
    install -d ${D}${sysconfdir}/connman
    install -m 644 ${UNPACKDIR}/main.conf ${D}${sysconfdir}/connman/main.conf
}
