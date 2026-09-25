FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://hoki-usb-bind"
do_install:append() {
    install -m 0755 ${UNPACKDIR}/hoki-usb-bind ${D}${bindir}/hoki-usb-bind
}
