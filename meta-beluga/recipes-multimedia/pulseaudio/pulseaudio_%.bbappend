# beluga's audio HAL cannot survive module-droid-card being dlclose()d.
do_install:append:beluga() {
    if ! grep -q '^load-module module-droid-card ' ${D}${sysconfdir}/pulse/default.pa; then
        bbfatal "default.pa: no 'load-module module-droid-card' line; beluga quirk not applied"
    fi
    sed -i '/^load-module module-droid-card /s/$/ quirks=+unload_call_exit/' \
        ${D}${sysconfdir}/pulse/default.pa
}
