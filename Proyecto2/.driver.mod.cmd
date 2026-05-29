savedcmd_driver.mod := printf '%s\n'   driver.o | awk '!x[$$0]++ { print("./"$$0) }' > driver.mod
