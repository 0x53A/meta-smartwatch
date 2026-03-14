# Use current build time instead of git commit timestamp,
# so we can identify which kernel build is running.
do_compile:prepend() {
    export SOURCE_DATE_EPOCH=$(date +%s)
}
