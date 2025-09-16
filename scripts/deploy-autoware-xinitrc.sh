#!/bin/bash
sudo cp autoware-xinitrc.sh /home/parcelpaler/.xinitrc
sudo chown parcelpaler:parcelpaler /home/parcelpaler/.xinitrc
sudo chmod +x /home/parcelpaler/.xinitrc

#Usage:
# Make the deployment script executable
# chmod +x deploy_xinitrc.sh

# Run the deployment
# ./deploy_xinitrc.sh