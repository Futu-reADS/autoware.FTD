#!/bin/bash

# Wait for network and graphics
sleep 10

# Export display for GUI applications
export DISPLAY=:0

# Set the workspace directory
WORKSPACE_DIR="/home/gary/autoware.FTD"
cd "$WORKSPACE_DIR" || exit 1

# Launch Autoware using the docker container in non-interactive mode
# update this in mini-pc
./docker/run.sh --map-path /path/to/your/map --headless "ros2 launch autoware_launch autoware.launch.xml map_path:=/autoware_map vehicle_model:=sample_vehicle sensor_model:=sample_sensor_kit"