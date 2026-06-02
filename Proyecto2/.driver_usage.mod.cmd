savedcmd_driver_usage.mod := printf '%s\n'   driver_usage.o | awk '!x[$$0]++ { print("./"$$0) }' > driver_usage.mod
