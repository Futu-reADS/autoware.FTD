#!/bin/sh
# Disable Ctrl+Alt+T and other keyboard shortcuts
gsettings set org.gnome.desktop.wm.keybindings panel-main-menu "[]"
gsettings set org.gnome.desktop.wm.keybindings switch-to-workspace-left "[]"
gsettings set org.gnome.desktop.wm.keybindings switch-to-workspace-right "[]"

# Hide desktop icons
gsettings set org.gnome.desktop.background show-desktop-icons false

# Start only what we need
exec /home/gary/autoware.FTD/scripts/autoware-startup.sh