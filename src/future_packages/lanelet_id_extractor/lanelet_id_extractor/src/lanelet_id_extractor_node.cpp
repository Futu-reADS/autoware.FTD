// Copyright 2024 Future Tools
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>
#include <vector>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "lanelet_id_extractor_msgs/msg/lanelet_info.hpp"

#include <lanelet2_core/LaneletMap.h>
#include <lanelet2_core/geometry/Lanelet.h>
#include <lanelet2_core/primitives/Lanelet.h>
#include <lanelet2_io/Io.h>
#include <lanelet2_projection/UTM.h>

namespace lanelet_id_extractor
{

class LaneletIdExtractorNode : public rclcpp::Node
{
public:
  explicit LaneletIdExtractorNode(const rclcpp::NodeOptions & options)
  : Node("lanelet_id_extractor", options),
    map_loaded_(false)
  {
    // Declare parameters
    declare_parameter("map_file_path", "");
    declare_parameter("pose_topic", "/localization/pose_twist_fusion_filter/pose");
    declare_parameter("output_topic", "/lanelet/current_info");
    declare_parameter("search_radius", 5.0);
    declare_parameter("publish_rate", 10.0);
    declare_parameter("frame_id", "map");

    // Get parameters
    map_file_path_ = get_parameter("map_file_path").as_string();
    const std::string pose_topic = get_parameter("pose_topic").as_string();
    const std::string output_topic = get_parameter("output_topic").as_string();
    search_radius_ = get_parameter("search_radius").as_double();
    publish_rate_ = get_parameter("publish_rate").as_double();
    frame_id_ = get_parameter("frame_id").as_string();

    // Validate map file path
    if (map_file_path_.empty()) {
      RCLCPP_ERROR(get_logger(), "map_file_path parameter is required!");
      return;
    }

    // Load the lanelet map
    if (!loadMap()) {
      RCLCPP_ERROR(get_logger(), "Failed to load map from: %s", map_file_path_.c_str());
      return;
    }

    // Create publisher
    publisher_ = create_publisher<lanelet_id_extractor_msgs::msg::LaneletInfo>(
      output_topic, 10);

    // Create subscriber
    pose_subscriber_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      pose_topic, 10,
      std::bind(&LaneletIdExtractorNode::poseCallback, this, std::placeholders::_1));

    // Setup rate limiting if enabled
    if (publish_rate_ > 0.0) {
      min_publish_interval_ = std::chrono::duration<double>(1.0 / publish_rate_);
    } else {
      min_publish_interval_ = std::chrono::duration<double>(0.0);
    }

    RCLCPP_INFO(get_logger(), "Lanelet ID Extractor Node initialized successfully");
    RCLCPP_INFO(get_logger(), "Map file: %s", map_file_path_.c_str());
    RCLCPP_INFO(get_logger(), "Loaded %zu lanelets", lanelet_map_->laneletLayer.size());
    RCLCPP_INFO(get_logger(), "Subscribing to: %s", pose_topic.c_str());
    RCLCPP_INFO(get_logger(), "Publishing to: %s", output_topic.c_str());
  }

private:
  bool loadMap()
  {
    try {
      RCLCPP_INFO(get_logger(), "Loading map from: %s", map_file_path_.c_str());

      // Create UTM projector (default for most Autoware maps)
      // You may need to adjust this based on your map's projection
      lanelet::projection::UtmProjector projector(lanelet::Origin({0.0, 0.0}));

      // Load the map
      lanelet_map_ = lanelet::load(map_file_path_, projector);

      if (!lanelet_map_) {
        RCLCPP_ERROR(get_logger(), "Failed to load lanelet map");
        return false;
      }

      if (lanelet_map_->laneletLayer.empty()) {
        RCLCPP_WARN(get_logger(), "Loaded map contains no lanelets");
      }

      map_loaded_ = true;
      return true;

    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), "Exception while loading map: %s", e.what());
      return false;
    }
  }

  void poseCallback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg)
  {
    if (!map_loaded_) {
      return;
    }

    // Rate limiting
    auto now = std::chrono::steady_clock::now();
    if ((now - last_publish_time_) < min_publish_interval_) {
      return;
    }

    // Extract position from pose
    const double x = msg->pose.pose.position.x;
    const double y = msg->pose.pose.position.y;
    lanelet::BasicPoint2d search_point(x, y);

    // Find the lanelet containing this point
    auto lanelet_info_msg = findLaneletAtPoint(search_point);

    // Set header
    lanelet_info_msg.header.stamp = msg->header.stamp;
    lanelet_info_msg.header.frame_id = frame_id_;

    // Publish
    publisher_->publish(lanelet_info_msg);
    last_publish_time_ = now;
  }

  lanelet_id_extractor_msgs::msg::LaneletInfo findLaneletAtPoint(
    const lanelet::BasicPoint2d & point)
  {
    lanelet_id_extractor_msgs::msg::LaneletInfo info_msg;

    // Default values (not in any lanelet)
    info_msg.lanelet_id = -1;
    info_msg.is_in_lanelet = false;

    try {
      // Search for lanelets within the search radius
      std::vector<std::pair<double, lanelet::Lanelet>> nearby_lanelets;

      for (const auto & lanelet : lanelet_map_->laneletLayer) {
        // Check if point is inside the lanelet polygon
        if (lanelet::geometry::inside(lanelet, point)) {
          // Point is inside this lanelet
          info_msg.lanelet_id = lanelet.id();
          info_msg.is_in_lanelet = true;
          extractLaneletAttributes(lanelet, info_msg);
          return info_msg;
        }

        // If not inside, calculate distance for fallback
        double distance = lanelet::geometry::distance2d(lanelet, point);
        if (distance < search_radius_) {
          nearby_lanelets.push_back({distance, lanelet});
        }
      }

      // If not inside any lanelet, use the nearest one within search radius
      if (!nearby_lanelets.empty()) {
        // Sort by distance
        std::sort(
          nearby_lanelets.begin(), nearby_lanelets.end(),
          [](const auto & a, const auto & b) { return a.first < b.first; });

        const auto & nearest = nearby_lanelets.front().second;
        info_msg.lanelet_id = nearest.id();
        info_msg.is_in_lanelet = false;  // Not inside, just nearby
        extractLaneletAttributes(nearest, info_msg);

        RCLCPP_DEBUG(
          get_logger(), "Vehicle near lanelet %ld (distance: %.2f m)",
          nearest.id(), nearby_lanelets.front().first);
      } else {
        RCLCPP_DEBUG(get_logger(), "No lanelet found within search radius");
      }

    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), "Error finding lanelet: %s", e.what());
    }

    return info_msg;
  }

  void extractLaneletAttributes(
    const lanelet::Lanelet & lanelet,
    lanelet_id_extractor_msgs::msg::LaneletInfo & info_msg)
  {
    // Extract common attributes
    info_msg.subtype = lanelet.attributeOr("subtype", "");
    info_msg.speed_limit = lanelet.attributeOr("speed_limit", "");
    info_msg.location = lanelet.attributeOr("location", "");
    info_msg.one_way = lanelet.attributeOr("one_way", "");
    info_msg.turn_direction = lanelet.attributeOr("turn_direction", "");

    // Extract all attributes as key-value pairs
    for (const auto & attr : lanelet.attributes()) {
      diagnostic_msgs::msg::KeyValue kv;
      kv.key = attr.first;
      kv.value = attr.second.value();
      info_msg.attributes.push_back(kv);
    }

    RCLCPP_DEBUG(
      get_logger(), "Found lanelet %ld: subtype=%s, speed_limit=%s",
      lanelet.id(), info_msg.subtype.c_str(), info_msg.speed_limit.c_str());
  }

  // Parameters
  std::string map_file_path_;
  double search_radius_;
  double publish_rate_;
  std::string frame_id_;

  // Lanelet map
  lanelet::LaneletMapPtr lanelet_map_;
  bool map_loaded_;

  // ROS2 interfaces
  rclcpp::Publisher<lanelet_id_extractor_msgs::msg::LaneletInfo>::SharedPtr publisher_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr
    pose_subscriber_;

  // Rate limiting
  std::chrono::steady_clock::time_point last_publish_time_;
  std::chrono::duration<double> min_publish_interval_;
};

}  // namespace lanelet_id_extractor

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(lanelet_id_extractor::LaneletIdExtractorNode)

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<lanelet_id_extractor::LaneletIdExtractorNode>(
    rclcpp::NodeOptions());
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
