#!/bin/sh
# On the phone: run a bionic program in /android/root with the vendor libraries first.
/root/android-root.sh
exec chroot /android/root /system/bin/env \
	LD_LIBRARY_PATH=/vendor/lib64:/vendor/lib64/hw:/system/lib64:/apex/com.android.runtime/lib64:/apex/com.android.i18n/lib64 "$@"
