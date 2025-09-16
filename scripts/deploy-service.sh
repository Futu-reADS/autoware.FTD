#!/bin/bash

sudo cp kiosk.service /etc/systemd/system/kiosk.service
sleep 1
sudo systemctl daemon-reload
sleep 1
sudo systemctl enable kiosk.service
sleep 1
sudo cp run-autoware.service /etc/systemd/system/run_autoware.service
sleep 1
sudo systemctl daemon-reload
sleep 1
sudo systemctl enable run_autoware.service