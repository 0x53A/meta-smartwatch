# Disable timed (Nemo/Sailfish time daemon) on hoki.
# Hoki uses systemd-timesyncd for NTP and the hardware RTC for persistence,
# so timed's phone-based time sync is not needed.
# The package is kept installed because other packages depend on libtimed.

do_install:append() {
    # Mask the service so it never starts
    ln -sf /dev/null ${D}/usr/lib/systemd/user/default.target.wants/timed-qt5.service
}
