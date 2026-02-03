# Lanelet ID Extractor - Usage Examples

## Quick Start

### 1. Build the Package

```bash
cd /path/to/autoware/workspace
colcon build --packages-up-to lanelet_id_extractor
source install/setup.bash
```

### 2. Prepare Your Map File

You need an OSM/Lanelet2 map file. The example OSM structure you provided:

```xml
<relation id="4650">
  <member type="way" role="left" ref="4648"/>
  <member type="way" role="right" ref="4649"/>
  <member type="way" role="centerline" ref="4675"/>
  <tag k="type" v="lanelet"/>
  <tag k="subtype" v="road"/>
  <tag k="speed_limit" v="4"/>
  <tag k="location" v="urban"/>
  <tag k="one_way" v="yes"/>
  <tag k="turn_direction" v="left"/>
</relation>
```

### 3. Run the Node

**Using launch file:**
```bash
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/your/map.osm
```

**Using ros2 run with parameters:**
```bash
ros2 run lanelet_id_extractor lanelet_id_extractor_node \
  --ros-args \
  -p map_file_path:=/path/to/your/map.osm \
  -p pose_topic:=/localization/pose_twist_fusion_filter/pose \
  -p output_topic:=/lanelet/current_info \
  -p search_radius:=5.0 \
  -p publish_rate:=10.0
```

### 4. Monitor Output

**View lanelet information:**
```bash
ros2 topic echo /lanelet/current_info
```

**Sample output:**
```yaml
header:
  stamp:
    sec: 1706777890
    nanosec: 123456789
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
---
```

## Integration with Autoware

### Typical Autoware Launch Integration

Add to your existing launch file:

```xml
<launch>
  <!-- Your existing Autoware nodes... -->

  <!-- Lanelet ID Extractor -->
  <include file="$(find-pkg-share lanelet_id_extractor)/launch/lanelet_id_extractor.launch.xml">
    <arg name="map_file_path" value="$(var map_path)"/>
    <arg name="pose_topic" value="/localization/pose_twist_fusion_filter/pose"/>
    <arg name="output_topic" value="/lanelet/current_info"/>
  </include>

  <!-- Your downstream nodes that use lanelet info... -->
</launch>
```

## Advanced Usage

### Custom Search Radius

Increase search radius for rural areas with sparse lanelets:

```bash
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/rural_map.osm \
  search_radius:=20.0
```

### Unlimited Publish Rate

For applications requiring immediate updates:

```bash
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/map.osm \
  publish_rate:=0.0
```

### Debug Mode

Enable debug logging to see detailed lanelet matching info:

```bash
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/map.osm \
  log_level:=debug
```

## Downstream Applications

### Example: Speed Limit Monitor

```cpp
#include "rclcpp/rclcpp.hpp"
#include "lanelet_id_extractor_msgs/msg/lanelet_info.hpp"

class SpeedLimitMonitor : public rclcpp::Node {
public:
  SpeedLimitMonitor() : Node("speed_limit_monitor") {
    subscription_ = create_subscription<lanelet_id_extractor_msgs::msg::LaneletInfo>(
      "/lanelet/current_info", 10,
      [this](const lanelet_id_extractor_msgs::msg::LaneletInfo::SharedPtr msg) {
        if (msg->is_in_lanelet && !msg->speed_limit.empty()) {
          float limit = std::stof(msg->speed_limit);
          RCLCPP_INFO(get_logger(), "Current speed limit: %.1f m/s", limit);
          // Compare with vehicle speed and warn if exceeding...
        }
      });
  }
private:
  rclcpp::Subscription<lanelet_id_extractor_msgs::msg::LaneletInfo>::SharedPtr subscription_;
};
```

### Example: Turn Direction Indicator

```python
import rclpy
from rclpy.node import Node
from lanelet_id_extractor_msgs.msg import LaneletInfo

class TurnIndicator(Node):
    def __init__(self):
        super().__init__('turn_indicator')
        self.subscription = self.create_subscription(
            LaneletInfo,
            '/lanelet/current_info',
            self.lanelet_callback,
            10)

    def lanelet_callback(self, msg):
        if msg.is_in_lanelet and msg.turn_direction:
            direction = msg.turn_direction
            if direction == 'left':
                self.get_logger().info('Approaching left turn')
                # Activate left turn signal...
            elif direction == 'right':
                self.get_logger().info('Approaching right turn')
                # Activate right turn signal...
            elif direction == 'straight':
                self.get_logger().info('Continue straight')

rclpy.init()
node = TurnIndicator()
rclpy.spin(node)
```

## Troubleshooting

### Node fails to start

**Error:** "map_file_path parameter is required!"
- **Solution:** Provide the map file path parameter

**Error:** "Failed to load map from: ..."
- **Solution:** Check file path is correct and file is valid OSM/Lanelet2 format

### No lanelet detected

**Symptom:** `lanelet_id: -1` and `is_in_lanelet: false`
- **Cause:** Vehicle is too far from any lanelet
- **Solution:**
  - Increase `search_radius` parameter
  - Verify map covers vehicle's current location
  - Check pose topic is publishing correct coordinates

### Incorrect lanelet detected

**Symptom:** Wrong lanelet ID reported
- **Cause:** Map projection mismatch
- **Solution:** Verify UTM projection in code matches your map's projection

## Testing Without Real Vehicle

### Using rosbag

```bash
# Play back recorded data
ros2 bag play /path/to/recorded_data.bag

# In another terminal
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/map.osm
```

### Manual Pose Publishing

```bash
# Publish test pose
ros2 topic pub /localization/pose_twist_fusion_filter/pose \
  geometry_msgs/msg/PoseWithCovarianceStamped \
  "{header: {frame_id: 'map'},
    pose: {pose: {position: {x: 100.0, y: 200.0, z: 0.0}}}}" \
  --once
```

## Performance Tuning

### Rate Limiting
- **High frequency localization (50Hz)** → Set `publish_rate: 10.0`
- **Low frequency localization (10Hz)** → Set `publish_rate: 0.0` (unlimited)

### Search Radius
- **Dense urban maps** → `search_radius: 3.0`
- **Highway maps** → `search_radius: 10.0`
- **Sparse rural maps** → `search_radius: 20.0`

## Message Interface

```
lanelet_id_extractor_msgs/msg/LaneletInfo

std_msgs/Header header
int64 lanelet_id           # -1 if not in/near any lanelet
bool is_in_lanelet         # true if inside lanelet polygon
string subtype             # "road", "crosswalk", "parking", etc.
string speed_limit         # Speed limit in m/s (as string)
string location            # "urban", "rural", "highway"
string one_way             # "yes" or "no"
string turn_direction      # "left", "right", "straight"
diagnostic_msgs/KeyValue[] attributes  # All OSM tags
```
