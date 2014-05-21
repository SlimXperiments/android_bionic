libc_bionic_src_files_arm += \
    arch-arm/cortex-a9/bionic/memcpy.S \
    arch-arm/cortex-a9/bionic/memset.S \
    arch-arm/cortex-a9/bionic/strcat.S \
    arch-arm/cortex-a9/bionic/strcmp.S \
    arch-arm/cortex-a9/bionic/strcpy.S \
    arch-arm/cortex-a9/bionic/strlen.S \
    arch-arm/cortex-a9/bionic/__strcat_chk.S \
    arch-arm/cortex-a9/bionic/__strcpy_chk.S \

ifeq ($(ARCH_ARM_HAVE_NEON),true)
    libc_bionic_src_files_arm += arch-arm/bionic/memmove.S
else
    libc_bionic_src_files_arm += \
        bionic/memmove.c \
        upstream-openbsd/lib/libc/string/bcopy.c
endif