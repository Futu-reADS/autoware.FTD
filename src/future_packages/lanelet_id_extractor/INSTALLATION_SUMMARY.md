# Lanelet ID Extractor - Installation Summary

## ✅ Successfully Created Packages

### Package Structure

```
src/future_packages/lanelet_id_extractor/
├── lanelet_id_extractor_msgs/      # Message definitions package
│   ├── msg/
│   │   └── LaneletInfo.msg         # Custom message with lanelet info
│   ├── CMakeLists.txt
│   └── package.xml
│
├── lanelet_id_extractor/           # Main node package
│   ├── src/
│   │   └── lanelet_id_extractor_node.cpp   # C++ node implementation
│   ├── launch/
│   │   └── lanelet_id_extractor.launch.xml # Launch file
│   ├── config/
│   │   └── lanelet_id_extractor.param.yaml # Parameter configuration
│   ├── CMakeLists.txt
│   └── package.xml
│
├── README.md                       # Complete documentation
├── USAGE_EXAMPLE.md               # Usage examples and integration guides
└── INSTALLATION_SUMMARY.md        # This file
```

## ✅ Build Status

Both packages compiled successfully:
- ✅ `lanelet_id_extractor_msgs` - Message package built
- ✅ `lanelet_id_extractor` - Node package built
- ✅ Executable installed at: `install/lanelet_id_extractor/lib/lanelet_id_extractor/lanelet_id_extractor_node`

## 📋 Package Details

### lanelet_id_extractor_msgs

**Message Definition:** `LaneletInfo.msg`

```
std_msgs/Header header
int64 lanelet_id           # ID of lanelet (-1 if not in any)
bool is_in_lanelet         # True if inside lanelet polygon
string subtype             # "road", "parking", "crosswalk"
string speed_limit         # Speed limit value
string location            # "urban", "rural"
string one_way             # "yes" or "no"
string turn_direction      # "left", "right", "straight"
diagnostic_msgs/KeyValue[] attributes  # All OSM tags
```

### lanelet_id_extractor

**Node:** `lanelet_id_extractor_node`

**Subscribed Topics:**
- `/localization/pose_twist_fusion_filter/pose` (geometry_msgs/msg/PoseWithCovarianceStamped)

**Published Topics:**
- `/lanelet/current_info` (lanelet_id_extractor_msgs/msg/LaneletInfo)

**Parameters:**
- `map_file_path` (string, required) - Path to OSM/Lanelet2 map file
- `pose_topic` (string, default: "/localization/pose_twist_fusion_filter/pose")
- `output_topic` (string, default: "/lanelet/current_info")
- `search_radius` (double, default: 5.0) - Search radius in meters
- `publish_rate` (double, default: 10.0) - Max publish rate in Hz
- `frame_id` (string, default: "map") - Output frame ID

## 🚀 Quick Start

### 1. Source the workspace
```bash
source install/setup.bash
```

### 2. Run the node
```bash
ros2 launch lanelet_id_extractor lanelet_id_extractor.launch.xml \
  map_file_path:=/path/to/your/map.osm
```

### 3. Monitor output
```bash
ros2 topic echo /lanelet/current_info
```

## 🔧 Implementation Details

### Algorithm
1. **Map Loading**: Loads OSM/Lanelet2 map at startup using Lanelet2 library
2. **Pose Subscription**: Subscribes to vehicle localization pose
3. **Lanelet Detection**:
   - Primary: Check if position is inside any lanelet polygon
   - Fallback: Find nearest lanelet within search radius
4. **Attribute Extraction**: Extract all OSM tags from the lanelet
5. **Publishing**: Publish LaneletInfo message with rate limiting

### Behavior When Not in Lanelet
- If nearest lanelet is within `search_radius`:
  - `is_in_lanelet = false`
  - `lanelet_id = nearest_lanelet_id`
  - Attributes from nearest lanelet
- If no lanelet within `search_radius`:
  - `is_in_lanelet = false`
  - `lanelet_id = -1`
  - All string fields empty

### Performance
- **Memory**: ~3.6 MB executable
- **CPU**: Minimal (optimized spatial queries)
- **Rate limiting**: Configurable to reduce overhead
- **Thread-safe**: Uses ROS2 executor model

## 📦 Dependencies

All dependencies are already available in Autoware:
- ✅ rclcpp
- ✅ geometry_msgs
- ✅ diagnostic_msgs
- ✅ std_msgs
- ✅ lanelet2_core
- ✅ lanelet2_io
- ✅ lanelet2_projection
- ✅ lanelet2_traffic_rules
- ✅ lanelet2_routing

## 🧪 Testing Recommendations

### Unit Tests (To be implemented)
```bash
# Test lanelet detection algorithm
# Test attribute extraction
# Test edge cases (no lanelet, multiple lanelets)
```

### Integration Testing
```bash
# Test with real Autoware localization
# Test with different map projections
# Test with rosbag playback
```

### Manual Testing
```bash
# 1. Start Autoware with localization
# 2. Launch lanelet_id_extractor
# 3. Drive vehicle and verify correct lanelet IDs
# 4. Check behavior at lanelet boundaries
```

## 📝 Example OSM Relation (from your spec)

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

**Output for this lanelet:**
```yaml
lanelet_id: 4650
is_in_lanelet: true
subtype: "road"
speed_limit: "4"
location: "urban"
one_way: "yes"
turn_direction: "left"
```

## 🎯 Use Cases

1. **Speed Limit Monitoring**: Extract current speed limit for vehicle
2. **Turn Prediction**: Get upcoming turn direction
3. **Navigation Context**: Understand road type (urban/rural)
4. **Behavior Planning**: Adjust behavior based on lanelet type
5. **HMI Display**: Show current road information to user
6. **Data Logging**: Record which lanelets were traversed

## 🔍 Troubleshooting

See `USAGE_EXAMPLE.md` for detailed troubleshooting guide.

Common issues:
- Map file path not provided → Add `map_file_path` parameter
- No lanelet detected → Increase `search_radius` parameter
- Wrong projection → Verify map projection matches UTM

## 📄 License

Apache-2.0 (matches Autoware license)

## 👥 Maintainer

Future Tools Team (dev@futu-re.co.jp)

## 🔗 Related Packages

- `lanelet2_extension` - Autoware's Lanelet2 extensions
- `map_loader` - Loads maps for Autoware
- `mission_planner` - Uses lanelets for route planning

---

**Status**: ✅ Ready for use and integration
**Version**: 0.1.0
**Last Updated**: 2024-02-03
