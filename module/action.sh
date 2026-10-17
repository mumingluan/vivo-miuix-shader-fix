#!/system/bin/sh
echo "=== vivo Miuix Shader Fix ==="
echo "Installed hooks in this boot:"
logcat -d -s VivoMiuixShaderFix:I '*:S' | tail -25
echo "Disable or uninstall in KernelSU; close and reopen the manager after unloading the Zygisk module."
