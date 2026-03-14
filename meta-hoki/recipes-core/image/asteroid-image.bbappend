# Postinstall intercepts need these native tools in the image sysroot
DEPENDS:append = " desktop-file-utils-native shared-mime-info-native"

# Dev/debug tools
IMAGE_INSTALL:append = " htop strace rsync gdbserver \
    evtest i2c-tools jq socat tcpdump lsof memtester devmem2 fbgrab tinyalsa vim-xxd"
