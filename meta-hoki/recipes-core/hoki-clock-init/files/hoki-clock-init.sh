#!/bin/sh
# hoki-clock-init: if system clock is before the build date, set it.
# This handles uninitialized/corrupted RTC values that would otherwise
# leave the system stuck at 1970, which crashes systemd.

BUILD_EPOCH="@BUILD_EPOCH@"

NOW=$(date -u +%s)

if [ "$NOW" -lt "$BUILD_EPOCH" ]; then
    date -u -s "@$BUILD_EPOCH" >/dev/null
    echo "hoki-clock-init: clock was $NOW, set to $BUILD_EPOCH ($(date -u))"
    # If hwclock is available and RTC is writable, persist it
    hwclock -w 2>/dev/null && echo "hoki-clock-init: wrote to RTC" || true
else
    echo "hoki-clock-init: clock OK ($NOW >= $BUILD_EPOCH)"
fi
