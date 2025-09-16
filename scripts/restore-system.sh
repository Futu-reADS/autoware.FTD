#!/bin/bash
set -e

echo "=== stop kiosk and autoware services ==="
sudo systemctl stop kiosk.service || true
sudo systemctl stop run_autoware.service || true

echo "=== disable service autostart ==="
sudo systemctl disable kiosk.service || true
sudo systemctl disable run_autoware.service || true

echo "=== remove service files ==="
sudo rm -f /etc/systemd/system/kiosk.service
sudo rm -f /etc/systemd/system/run_autoware.service
sudo systemctl daemon-reload

echo "=== restore virtual terminals (TTY) ==="
for i in 2 3 4 5 6; do
  sudo systemctl unmask [email protected] || true
done
sudo sed -i -e 's/^NAutoVTs=0/#NAutoVTs=6/' -e 's/^ReserveVT=1/#ReserveVT=1/' /etc/systemd/logind.conf || true
sudo systemctl restart systemd-logind

echo "=== restore graphical login manager ==="
# If you are using gdm3 (default for Ubuntu Desktop), enable it
if systemctl list-unit-files | grep -q gdm3.service; then
  sudo systemctl enable gdm3.service
  sudo systemctl set-default graphical.target
  echo "Enabled gdm3 as the default GUI login manager"
elif systemctl list-unit-files | grep -q lightdm.service; then
  sudo systemctl enable lightdm.service
  sudo systemctl set-default graphical.target
  echo "Enabled lightdm as the default GUI login manager"
else
  echo "did not detected gdm3/lightdm， please manually enable your display manager"
fi

echo "=== remove parcelpaler user (if only used for kiosk) ==="
sudo deluser --remove-home parcelpaler

echo "=== done, a reboot is recommended ==="
echo "Execute sudo reboot to re-enter the normal GUI login"

#Usage:
# chmod +x restore-system.sh
# ./restore-system.sh
# sudo reboot