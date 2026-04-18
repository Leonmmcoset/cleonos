CLeonOS ramdisk root layout

/system  : kernel-mode ELF apps and core system components
/shell   : user shell and command ELF apps
/temp    : runtime temp/cache files
/driver  : hardware and peripheral drivers
/dev     : device interface nodes (/dev/tty, /dev/null, /dev/zero, /dev/random)

Root ELF demos:
/hello.elf     : Hello world user ELF
