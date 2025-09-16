#!/bin/sh
# disable DPMS / screen blanking
xset s off
xset -dpms
xset s noblank

# start openbox (lightweight WM)
openbox-session &

# small delay to ensure X is ready
sleep 5

# launch Chromium in kiosk mode
exec /usr/bin/chromium-browser \
  --noerrdialogs \
  --disable-infobars \
  --disable-translate \
  --disable-session-crashed-bubble \
  --kiosk "http://localhost:3000" \
  --incognito \
  --no-first-run \
  --user-data-dir=/home/parcelpaler/.config/chromium-kiosk




