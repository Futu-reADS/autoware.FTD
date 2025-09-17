#!/bin/bash

# prep-setting:
# Make the script executable
chmod +x prep-setting.sh
# Run the script
./prep-setting.sh

# Set up autoware-xinitrc:
# Make the deployment script executable
chmod +x deploy-autoware-xinitrc.sh
# Run the deployment
./deploy-autoware-xinitrc.sh

# Set up deploy-service:
# Make the deployment script executable
chmod +x deploy-service.sh
# Run the deployment
./deploy-service.sh

# Disable TTYs and virtual console switching:
# Make the script executable
chmod +x disable-TTYs-and-virtual-console-switching
# Run the script
./disable-TTYs-and-virtual-console-switching

# Usage:
# chmod +x autoware-set-up.sh
# ./autoware-set-up.sh
# warning : TRY IT ON A TEST MACHINE OR VIRTUAL MACHINE !!!!!!