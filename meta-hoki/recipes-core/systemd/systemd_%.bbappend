# Re-enable systemd-timesyncd on hoki.
# Upstream AsteroidOS disables it because it clashes with timed (Nemo phone
# time sync). Hoki doesn't use timed, so timesyncd is the NTP client.
PACKAGECONFIG:append = " timesyncd"
