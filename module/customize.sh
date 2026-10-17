#!/system/bin/sh
[ "$ARCH" = arm64 ] || abort "This build requires arm64."
[ "$API" -ge 33 ] || abort "This build requires Android 13 or later."
ui_print "- vivo Miuix Shader Fix"
ui_print "- Requires Zygisk / Zygisk Next"
ui_print "- Targets com.android.shell only"
ui_print "- LSPosed APKs and system partitions are unchanged"
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/action.sh" 0 0 0755
