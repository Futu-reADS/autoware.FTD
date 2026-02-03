# Lanelet ID Extractor

ROS2 package for extracting lanelet ID and properties from vehicle position in real-time.

## Documentation

- **[System Operation Guide](docs/SYSTEM_OPERATION.md)** - Detailed explanation of how the node works, processing algorithms, and output rate behavior
- **[Usage Examples](USAGE_EXAMPLE.md)** - Practical examples and integration guides
- **[Installation Summary](INSTALLATION_SUMMARY.md)** - Build instructions and package overview

## Overview

This package provides a node that:
- Subscribes to vehicle pose from localization
- Loads an OSM/Lanelet2 map file
- Determines which lanelet the vehicle is currently in
- Publishes lanelet ID and all associated properties (subtype, speed_limit, location, etc.)

## Features

- **Real-time lanelet detection**: Determines if vehicle is inside a lanelet polygon
- **Nearest lanelet fallback**: If not in any lanelet, finds the nearest one within search radius
- **Comprehensive attributes**: Extracts all OSM tags (subtype, speed_limit, location, turn_direction, etc.)
- **Rate limiting**: Configurable publish rate to reduce overhead
- **Flexible output**: Both explicit fields for common attributes and key-value pairs for extensibility

## Packages

### lanelet_id_extractor_msgs
Message definitions for lanelet information.

**Messages:**
- `LaneletInfo.msg`: Contains lanelet ID, validation flag, common attributes, and flexible key-value pairs

### lanelet_id_extractor
Main node implementation.

## Building

```bash
# From workspace root
colcon build --packages-up-to lanelet_id_extractor
source install/setup.bash
```

## Usage

### Launch File

```bash
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/your/map.osm \
  pose_topic:=/localization/pose_twist_fusion_filter/pose \
  output_topic:=/lanelet/current_info \
  search_radius:=5.0 \
  publish_rate:=10.0
```

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `map_file_path` | string | "" | **Required** Path to OSM/Lanelet2 map file |
| `pose_topic` | string | `/localization/pose_twist_fusion_filter/pose` | Input pose topic |
| `output_topic` | string | `/lanelet/current_info` | Output lanelet info topic |
| `search_radius` | double | 5.0 | Search radius for nearest lanelet (meters) |
| `publish_rate` | double | 10.0 | Max publish rate (Hz), 0 for unlimited |
| `frame_id` | string | "map" | Frame ID for output messages |

### Topics

**Subscribed:**
- `{pose_topic}` (`geometry_msgs/msg/PoseWithCovarianceStamped`): Vehicle pose from localization

**Published:**
- `{output_topic}` (`lanelet_id_extractor_msgs/msg/LaneletInfo`): Current lanelet information

### Example Output Message

```yaml
header:
  stamp: {sec: 1234567890, nanosec: 0}
  frame_id: "map"
lanelet_id: 4650
is_in_lanelet: true
subtype: "road"
speed_limit: "4"
location: "urban"
one_way: "yes"
turn_direction: "left"
attributes:
  - key: "type"
    value: "lanelet"
  - key: "subtype"
    value: "road"
  - key: "speed_limit"
    value: "4"
  - key: "location"
    value: "urban"
  - key: "one_way"
    value: "yes"
  - key: "turn_direction"
    value: "left"
```

## Behavior

### When Vehicle is Inside a Lanelet
- `is_in_lanelet`: `true`
- `lanelet_id`: Actual lanelet ID
- All attributes are populated from the lanelet's OSM tags

### When Vehicle is NOT in Any Lanelet
- If nearest lanelet is within `search_radius`:
  - `is_in_lanelet`: `false`
  - `lanelet_id`: Nearest lanelet ID
  - Attributes from nearest lanelet
- If no lanelet within `search_radius`:
  - `is_in_lanelet`: `false`
  - `lanelet_id`: `-1`
  - All string attributes are empty

## Architecture

```
┌─────────────────────────────────────────┐
│  Localization (EKF/NDT)                 │
│  /localization/pose_twist_fusion_filter │
└──────────────┬──────────────────────────┘
               │ PoseWithCovarianceStamped
               ↓
┌─────────────────────────────────────────┐
│  Lanelet ID Extractor Node              │
│                                         │
│  1. Load OSM map at startup             │
│  2. On each pose update:                │
│     - Check if inside any lanelet       │
│     - Fallback to nearest if not inside │
│     - Extract all attributes            │
│  3. Publish LaneletInfo                 │
└──────────────┬──────────────────────────┘
               │ LaneletInfo
               ↓
┌─────────────────────────────────────────┐
│  Your Application                       │
│  (e.g., speed limit monitor,            │
│   turn prediction, behavior planning)   │
└─────────────────────────────────────────┘
```

## Dependencies

- ROS2 Humble or later
- Lanelet2 library
- geometry_msgs
- diagnostic_msgs
- std_msgs

## Testing

```bash
# Echo output topic
ros2 topic echo /lanelet/current_info

# Check if node is running
ros2 node list | grep lanelet_id_extractor

# View node info
ros2 node info /lanelet_id_extractor
```

## Notes

- The node uses `lanelet::geometry::inside()` for precise polygon containment checks
- UTM projection is used by default (adjust in code if your map uses different projection)
- Rate limiting prevents excessive publishing; set to 0 to publish on every pose update
- The `attributes` array contains ALL OSM tags, including duplicates of the explicit fields

## License

Apache-2.0
