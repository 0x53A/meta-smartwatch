# Fix CRLF line endings in upstream tarball that break the shebang
do_install:prepend() {
    find ${S} -name '*.py' -exec sed -i 's/\r$//' {} +
}
