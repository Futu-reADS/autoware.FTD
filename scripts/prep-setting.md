# update (Optional)
sudo apt update && sudo apt upgrade -y

# create a dedicated kiosk user (no interactive password prompt)
sudo adduser --disabled-password --gecos "" parcelpaler

# give kiosk access to video/audio (if needed)
sudo usermod -aG video,audio parcelpaler

# install minimal X + openbox + xinit + chromium (or Google Chrome)
# NOTE: on Ubuntu 22.04 'chromium' may be a snap; if you prefer the deb package use google-chrome .deb instead.
sudo apt install -y xorg openbox xinit x11-xserver-utils unclutter xdotool

# install chromium (snap on 22.04)
sudo apt install -y chromium-browser
