obj-m := b0slog.o
# TWRP's kernel (clang 12) uses cross-DSO CFI, not kCFI: modules need __cfi_check
ccflags-y += -fsanitize=cfi -fsanitize-cfi-cross-dso -fno-sanitize-cfi-canonical-jump-tables -fsanitize-trap=cfi
