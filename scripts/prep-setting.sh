#!/bin/bash
# update (Optional)
sudo apt update && sudo apt upgrade -y
sleep 1
# create a dedicated kiosk user (no interactive password prompt)
sudo adduser --disabled-password --gecos "" parcelpaler
sleep 1
# give kiosk access to video/audio (if needed)
sudo usermod -aG video,audio parcelpaler
sleep 1
# install minimal X + openbox + xinit + chromium (or Google Chrome)
# NOTE: on Ubuntu 22.04 'chromium' may be a snap; if you prefer the deb package use google-chrome .deb instead.
sudo apt install -y xorg openbox xinit x11-xserver-utils unclutter xdotool
sleep 1
# install chromium (snap on 22.04)
sudo apt install -y chromium-browser


