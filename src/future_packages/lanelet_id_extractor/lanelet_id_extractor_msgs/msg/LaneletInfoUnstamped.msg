# Message containing current lanelet information without header
# This is the unstamped version of LaneletInfo

# Lanelet identification
int64 lanelet_id          # ID of the lanelet (-1 if not in any lanelet)
bool is_in_lanelet        # True if vehicle is currently in a valid lanelet

# Common lanelet attributes (empty if not in lanelet)
string subtype            # e.g., "road", "parking", "crosswalk"
string speed_limit        # Speed limit in the lanelet
string location           # e.g., "urban", "rural"
string one_way            # "yes" or "no"
string turn_direction     # e.g., "left", "right", "straight"

# Additional attributes as key-value pairs
diagnostic_msgs/KeyValue[] attributes
