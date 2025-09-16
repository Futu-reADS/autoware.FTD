#!/bin/bash

# prep-setting:
# Make the script executable
chmod +x prep-setting.sh
# Run the script
./prep-setting.sh

# Set up autoware-xinitrc:
# Make the deployment script executable
chmod +x deploy_xinitrc.sh
# Run the deployment
./deploy_xinitrc.sh

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