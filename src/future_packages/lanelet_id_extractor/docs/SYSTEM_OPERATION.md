# Lanelet ID Extractor - System Operation

## Overview

The Lanelet ID Extractor determines which lanelet (lane segment) the vehicle is currently in by:
1. Loading an OSM/Lanelet2 map at startup
2. Subscribing to vehicle pose from localization
3. Finding which lanelet contains the vehicle position
4. Publishing lanelet ID and properties

**Key Principle**: Reactive processing - each pose message triggers lanelet detection and publishing (subject to rate limiting).

---

## System Initialization

### Startup Sequence

```
1. Load Parameters
   ├─ map_file_path (required)
   ├─ search_radius (default: 5.0m)
   └─ publish_rate (default: 10.0 Hz)

2. Load OSM Map
   ├─ Parse XML using Lanelet2 library
   ├─ Build lanelet polygons from OSM relations
   └─ Create spatial index for fast queries
   └─ Result: lanelet_map_ stored in memory

3. Setup ROS2 Interfaces
   ├─ Publisher: /lanelet/current_info
   └─ Subscriber: /localization/pose_twist_fusion_filter/pose

4. Configure Rate Limiter
   └─ min_publish_interval = 1.0 / publish_rate

5. Node Ready (waiting for pose messages)
```

**Memory**: Map loaded once at startup (~4MB + map size), remains read-only during operation.

---

## Runtime Processing

### Processing Flow (Per Pose Message)

```cpp
poseCallback(PoseMsg) {
    // STEP 1: Pre-checks
    if (!map_loaded_) return;

    // STEP 2: Rate limiting
    if (time_since_last_publish < min_interval) {
        return; // SKIP this message
    }

    // STEP 3: Extract position
    x = msg->pose.position.x;
    y = msg->pose.position.y;

    // STEP 4: Find lanelet (see algorithm below)
    lanelet_info = findLaneletAtPoint(x, y);

    // STEP 5: Publish
    publish(lanelet_info);
    last_publish_time = now;
}
```

**Processing Time**: 1-5ms typical (depends on map size and vehicle location)

---

## Output Rate Behavior

### Rate Control Formula

```
Actual Output Rate = MIN(Input Pose Rate, publish_rate parameter)
```

### How Rate Limiting Works

```
Example: publish_rate = 10 Hz (100ms minimum interval)

Time  | Pose Input | Time Since Last | Action
------|------------|-----------------|------------------
0 ms  | ✓          | -               | ✓ PUBLISH (first msg)
20 ms | ✓          | 20ms < 100ms    | ✗ SKIP
40 ms | ✓          | 40ms < 100ms    | ✗ SKIP
60 ms | ✓          | 60ms < 100ms    | ✗ SKIP
80 ms | ✓          | 80ms < 100ms    | ✗ SKIP
100ms | ✓          | 100ms >= 100ms  | ✓ PUBLISH
120ms | ✓          | 20ms < 100ms    | ✗ SKIP
200ms | ✓          | 100ms >= 100ms  | ✓ PUBLISH
```

**Important**: Skipped messages are discarded (no queuing).

### Rate Examples

| Localization Rate | publish_rate | Actual Output | Messages Skipped |
|-------------------|--------------|---------------|------------------|
| 50 Hz             | 10 Hz        | 10 Hz         | 80% (4 of 5)     |
| 50 Hz             | 0 (unlimited)| 50 Hz         | 0%               |
| 10 Hz             | 50 Hz        | 10 Hz         | 0% (input limit) |
| 100 Hz            | 20 Hz        | 20 Hz         | 80% (4 of 5)     |

---

## Lanelet Detection Algorithm

### Two-Phase Search Strategy

```
Phase 1: EXACT MATCH (Check if vehicle is INSIDE a lanelet)
├─ Loop through all lanelets in map
├─ For each lanelet:
│  ├─ Check: lanelet::geometry::inside(lanelet, point)
│  │  Uses ray-casting algorithm (point-in-polygon test)
│  └─ If INSIDE:
│     ├─ Extract all attributes
│     ├─ Set is_in_lanelet = TRUE
│     └─ RETURN (first match wins, stop searching)
└─ If no match found → Continue to Phase 2

Phase 2: NEAREST LANELET (Fallback when not inside any)
├─ Loop through all lanelets again
├─ Calculate distance to each lanelet centerline
├─ Filter: Keep only lanelets within search_radius
├─ Sort by distance (closest first)
└─ If nearest found:
   ├─ Extract attributes from nearest lanelet
   ├─ Set is_in_lanelet = FALSE
   └─ RETURN
└─ If none found within radius:
   ├─ Set lanelet_id = -1
   ├─ Set is_in_lanelet = FALSE
   └─ RETURN (empty attributes)
```

### Geometric Algorithms

**Point-in-Polygon (Phase 1)**:
- Draw ray from point to infinity
- Count intersections with polygon edges
- Odd intersections = INSIDE, Even = OUTSIDE

**Distance Calculation (Phase 2)**:
- Find closest point on lanelet centerline
- Calculate Euclidean distance: `sqrt((x2-x1)² + (y2-y1)²)`

---

## Output States

The node produces three distinct output states:

### State 1: Inside Lanelet (Exact Match)

**Condition**: Vehicle position is inside lanelet polygon

```yaml
lanelet_id: 4650              # Actual lanelet ID
is_in_lanelet: true           # TRUE = inside
subtype: "road"               # From OSM tags
speed_limit: "4"
location: "urban"
one_way: "yes"
turn_direction: "left"
```

**Use Cases**: Speed limit enforcement, lane keeping, turn signals

---

### State 2: Near Lanelet (Fallback)

**Condition**: Not inside any lanelet, but nearest within search_radius

```yaml
lanelet_id: 4650              # Nearest lanelet ID
is_in_lanelet: false          # FALSE = not inside
subtype: "road"               # From nearest lanelet
speed_limit: "4"
# ... attributes from nearest
```

**Typical Scenarios**: Lane change, slight GPS error, approaching intersection

**Use Cases**: Predictive information, graceful degradation

---

### State 3: Off-Road (No Lanelet)

**Condition**: No lanelet within search_radius

```yaml
lanelet_id: -1                # -1 = no lanelet found
is_in_lanelet: false          # FALSE
subtype: ""                   # Empty strings
speed_limit: ""
# ... all attributes empty
```

**Typical Scenarios**: Unmapped area, parking lot, large localization error

**Use Cases**: Disable lanelet features, trigger off-map warning

---

### State Transition Example

```
Time | Vehicle Position     | State   | Output
-----|---------------------|---------|------------------
0s   | Inside lane 1       | State 1 | id=4650, inside=true
1s   | Inside lane 1       | State 1 | id=4650, inside=true
2s   | Lane change starts  | State 2 | id=4650, inside=false
3s   | Between lanes       | State 2 | id=4651, inside=false
4s   | Inside lane 2       | State 1 | id=4651, inside=true
5s   | Exit to parking     | State 2 | id=4651, inside=false
6s   | Deep in parking lot | State 3 | id=-1, inside=false
```

---

## Performance Characteristics

### CPU Usage

| Scenario              | Time per Message | Notes                    |
|-----------------------|------------------|--------------------------|
| Inside first lanelet  | 0.1-0.5 ms       | Best case (early exit)   |
| Average case          | 1-5 ms           | Check 10-50 lanelets     |
| Large map worst case  | 10-50 ms         | Depends on map size      |

**CPU Load Example** (500 lanelets, 50 Hz input, 10 Hz output):
- Processing: 10 msg/s × 3 ms/msg = 30 ms/s
- CPU usage: ~3%

### Memory Usage

```
Static (at startup):
├─ Executable: 3.6 MB
├─ Map data: 1-50 MB (depends on OSM file size)
└─ Per lanelet: ~1-5 KB

Runtime (per message):
├─ Input/output messages: ~1.5 KB
└─ Temporary buffers: ~5 KB
    (freed after publish)
```

### Latency

End-to-end (pose received → lanelet info published): **1.5-6 ms** typical

---

## Configuration Guidelines

### Standard Configuration
```yaml
map_file_path: "/path/to/map.osm"  # Required
search_radius: 5.0                 # Meters
publish_rate: 10.0                 # Hz (0 = unlimited)
```

**Use Case**: Most applications (HMI, planning)

---

### High-Precision Configuration
```yaml
search_radius: 3.0                 # Tighter for dense urban
publish_rate: 0.0                  # No rate limiting
```

**Use Case**: Safety-critical, immediate response needed

---

### Low-Resource Configuration
```yaml
search_radius: 10.0                # Larger for highways
publish_rate: 5.0                  # Lower rate
```

**Use Case**: Embedded systems, resource-constrained platforms

---

## Troubleshooting

### Problem: No Output Published

**Diagnosis**:
```bash
ros2 topic echo /lanelet/current_info
ros2 node list | grep lanelet_id_extractor
```

**Solutions**:
- Check map loaded successfully (node logs)
- Verify localization is publishing poses
- Check `publish_rate` isn't too restrictive

---

### Problem: Always Returns `lanelet_id = -1`

**Diagnosis**:
```bash
ros2 topic echo /localization/pose_twist_fusion_filter/pose
```

**Solutions**:
- Increase `search_radius` (try 10.0 or 20.0)
- Verify vehicle position is within map bounds
- Check map projection matches localization frame (UTM)

---

### Problem: Wrong Lanelet Detected

**Diagnosis**:
```bash
# Enable debug logging
ros2 run lanelet_id_extractor lanelet_id_extractor_node \
  --ros-args --log-level debug
```

**Solutions**:
- Check for overlapping lanelets in map (first match wins)
- Expected behavior at lane boundaries
- Verify map quality

---

## Key Takeaways

1. **Output Rate**: `MIN(input pose rate, publish_rate)`
   - Set `publish_rate=0` for unlimited (every message)
   - Rate limiting discards messages, no queuing

2. **Three Output States**:
   - Inside lanelet: `is_in_lanelet=true`
   - Near lanelet: `is_in_lanelet=false, id ≠ -1`
   - Off-road: `is_in_lanelet=false, id = -1`

3. **Search Strategy**:
   - Phase 1: Check inside (exact match)
   - Phase 2: Find nearest (fallback)
   - First inside match wins (stops searching)

4. **Performance**:
   - Latency: 1.5-6 ms typical
   - CPU: 1-15% (depends on config)
   - Map loaded once (static)

5. **Configuration**:
   - Increase `search_radius` for sparse maps/highways
   - Set `publish_rate` based on downstream needs
   - Use debug logging for troubleshooting

---

**Document Version**: 1.0
**Last Updated**: 2024-02-03
**Node Version**: 0.1.0
