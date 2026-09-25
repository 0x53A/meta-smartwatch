# The hal-droid service name is correct, but the vendor executable has a
# -qti suffix. Patch the file in its owning package so the legacy HAL works
# in the base Hoki image; custom images can remove this launcher entirely.
PR:append:hoki = ".gnss1"

do_install:append:hoki() {
    rc=${D}${libexecdir}/hal-droid/system/etc/init/android.hardware.gnss@1.0-service.rc
    old='service vendor.gnss_service /vendor/bin/hw/android.hardware.gnss@1.0-service'
    new='service vendor.gnss_service /vendor/bin/hw/android.hardware.gnss@1.0-service-qti'

    [ -f "$rc" ] || bbfatal "Missing Hoki GNSS service definition: $rc"
    grep -qxF "$old" "$rc" || bbfatal "Unexpected Hoki GNSS service definition: $rc"
    sed -i "s|^$old$|$new|" "$rc"
    grep -qxF "$new" "$rc" || bbfatal "Failed to select Hoki QTI GNSS service: $rc"
}
