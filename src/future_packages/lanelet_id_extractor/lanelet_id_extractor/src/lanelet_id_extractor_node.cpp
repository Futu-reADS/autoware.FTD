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
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "lanelet_id_extractor_msgs/msg/lanelet_info.hpp"
#include "lanelet_id_extractor_msgs/msg/lanelet_info_unstamped.hpp"

#include <lanelet2_core/LaneletMap.h>
#include <lanelet2_core/geometry/Lanelet.h>
#include <lanelet2_core/primitives/Lanelet.h>
#include <lanelet2_io/Io.h>
#include <lanelet2_io/Projection.h>

namespace lanelet_id_extractor
{

// Local projector for maps with local_x/local_y attributes
// This matches Autoware's map_loader implementation
namespace projection
{
class LocalProjector : public lanelet::Projector
{
public:
  LocalProjector() : Projector(lanelet::Origin(lanelet::GPSPoint{})) {}

  lanelet::BasicPoint3d forward(const lanelet::GPSPoint & gps) const override
  {
    return lanelet::BasicPoint3d{0.0, 0.0, gps.ele};
  }

  lanelet::GPSPoint reverse(const lanelet::BasicPoint3d & point) const override
  {
    return lanelet::GPSPoint{0.0, 0.0, point.z()};
  }
};
}  // namespace projection

class LaneletIdExtractorNode : public rclcpp::Node
{
public:
  explicit LaneletIdExtractorNode(const rclcpp::NodeOptions & options)
  : Node("lanelet_id_extractor", options),
    map_loaded_(false),
    last_lanelet_id_(-1)
  {
    // Declare parameters
    declare_parameter("map_file_path", "");
    declare_parameter("input_pose_type", "PoseWithCovarianceStamped");
    declare_parameter("search_radius", 5.0);
    declare_parameter("publish_rate", 10.0);
    declare_parameter("frame_id", "map");

    // Get parameters
    map_file_path_ = get_parameter("map_file_path").as_string();
    input_pose_type_ = get_parameter("input_pose_type").as_string();
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

    // Create dual publishers (stamped and unstamped)
    publisher_stamped_ = create_publisher<lanelet_id_extractor_msgs::msg::LaneletInfo>(
      "~/output/stamped", 10);
    publisher_unstamped_ = create_publisher<lanelet_id_extractor_msgs::msg::LaneletInfoUnstamped>(
      "~/output/unstamped", 10);

    // Create subscriber based on input_pose_type parameter
    createSubscription();

    // Setup rate limiting if enabled
    if (publish_rate_ > 0.0) {
      min_publish_interval_ = std::chrono::duration<double>(1.0 / publish_rate_);
    } else {
      min_publish_interval_ = std::chrono::duration<double>(0.0);
    }

    RCLCPP_INFO(get_logger(), "Lanelet ID Extractor initialized with %zu lanelets",
                lanelet_map_->laneletLayer.size());
  }

private:
  bool loadMap()
  {
    try {
      RCLCPP_INFO(get_logger(), "Loading map from: %s", map_file_path_.c_str());

      // Use LocalProjector which works with maps that have local_x/local_y attributes
      // This matches how Autoware's map_loader handles simulator maps
      projection::LocalProjector projector;

      // Load the map
      lanelet::ErrorMessages errors;
      lanelet_map_ = lanelet::load(map_file_path_, projector, &errors);

      if (!errors.empty()) {
        for (const auto & error : errors) {
          RCLCPP_WARN(get_logger(), "Map loading warning: %s", error.c_str());
        }
      }

      if (!lanelet_map_) {
        RCLCPP_ERROR(get_logger(), "Failed to load lanelet map");
        return false;
      }

      if (lanelet_map_->laneletLayer.empty()) {
        RCLCPP_WARN(get_logger(), "Loaded map contains no lanelets");
        return false;
      }

      // For maps with local_x/local_y attributes, overwrite coordinates
      // This ensures consistency with Autoware's coordinate system
      for (lanelet::Point3d point : lanelet_map_->pointLayer) {
        if (point.hasAttribute("local_x") && point.hasAttribute("local_y")) {
          point.x() = point.attribute("local_x").asDouble().value();
          point.y() = point.attribute("local_y").asDouble().value();
        }
      }

      // Realign lanelet borders using updated points
      for (lanelet::Lanelet lanelet : lanelet_map_->laneletLayer) {
        auto left = lanelet.leftBound();
        auto right = lanelet.rightBound();
        std::tie(left, right) = lanelet::geometry::align(left, right);
        lanelet.setLeftBound(left);
        lanelet.setRightBound(right);
      }

      map_loaded_ = true;
      return true;

    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), "Exception while loading map: %s", e.what());
      return false;
    }
  }

  void createSubscription()
  {
    if (input_pose_type_ == "PoseWithCovarianceStamped") {
      pose_sub_pwcs_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
        "~/input/pose", 10,
        std::bind(&LaneletIdExtractorNode::poseCallbackPWCS, this, std::placeholders::_1));
      RCLCPP_INFO(get_logger(), "Subscribed to PoseWithCovarianceStamped on ~/input/pose");
    } else if (input_pose_type_ == "PoseStamped") {
      pose_sub_ps_ = create_subscription<geometry_msgs::msg::PoseStamped>(
        "~/input/pose", 10,
        std::bind(&LaneletIdExtractorNode::poseCallbackPS, this, std::placeholders::_1));
      RCLCPP_INFO(get_logger(), "Subscribed to PoseStamped on ~/input/pose");
    } else if (input_pose_type_ == "PoseWithCovariance") {
      pose_sub_pwc_ = create_subscription<geometry_msgs::msg::PoseWithCovariance>(
        "~/input/pose", 10,
        std::bind(&LaneletIdExtractorNode::poseCallbackPWC, this, std::placeholders::_1));
      RCLCPP_INFO(get_logger(), "Subscribed to PoseWithCovariance on ~/input/pose");
    } else if (input_pose_type_ == "Pose") {
      pose_sub_p_ = create_subscription<geometry_msgs::msg::Pose>(
        "~/input/pose", 10,
        std::bind(&LaneletIdExtractorNode::poseCallbackP, this, std::placeholders::_1));
      RCLCPP_INFO(get_logger(), "Subscribed to Pose on ~/input/pose");
    } else {
      RCLCPP_ERROR(get_logger(), "Invalid input_pose_type: %s. Valid types: PoseWithCovarianceStamped, PoseStamped, PoseWithCovariance, Pose",
                   input_pose_type_.c_str());
      throw std::runtime_error("Invalid input_pose_type parameter");
    }
  }

  // Callback for PoseWithCovarianceStamped
  void poseCallbackPWCS(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg)
  {
    processPose(msg->pose.pose, msg->header.stamp, msg->header.frame_id);
  }

  // Callback for PoseStamped
  void poseCallbackPS(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
  {
    processPose(msg->pose, msg->header.stamp, msg->header.frame_id);
  }

  // Callback for PoseWithCovariance (no header - generate timestamp)
  void poseCallbackPWC(const geometry_msgs::msg::PoseWithCovariance::SharedPtr msg)
  {
    processPose(msg->pose, this->now(), frame_id_);
  }

  // Callback for Pose (no header - generate timestamp)
  void poseCallbackP(const geometry_msgs::msg::Pose::SharedPtr msg)
  {
    processPose(*msg, this->now(), frame_id_);
  }

  // Common processing for all pose types
  void processPose(const geometry_msgs::msg::Pose & pose,
                   const rclcpp::Time & timestamp,
                   const std::string & frame_id)
  {
    if (!map_loaded_) {
      static bool warned = false;
      if (!warned) {
        RCLCPP_WARN(get_logger(), "Received pose but map not loaded yet");
        warned = true;
      }
      return;
    }

    // Rate limiting
    auto now = std::chrono::steady_clock::now();
    if ((now - last_publish_time_) < min_publish_interval_) {
      return;
    }

    // Extract position from pose
    const double x = pose.position.x;
    const double y = pose.position.y;
    lanelet::BasicPoint2d search_point(x, y);

    // Find the lanelet containing this point (returns unstamped version)
    auto lanelet_info_unstamped = findLaneletAtPoint(search_point);

    // Update last lanelet ID
    last_lanelet_id_ = lanelet_info_unstamped.lanelet_id;

    // Create stamped version with header
    lanelet_id_extractor_msgs::msg::LaneletInfo lanelet_info_stamped;
    lanelet_info_stamped.header.stamp = timestamp;
    lanelet_info_stamped.header.frame_id = frame_id;
    copyLaneletInfo(lanelet_info_unstamped, lanelet_info_stamped);

    // Publish both versions
    publisher_stamped_->publish(lanelet_info_stamped);
    publisher_unstamped_->publish(lanelet_info_unstamped);
    last_publish_time_ = now;
  }

  // Helper to copy unstamped data to stamped message
  void copyLaneletInfo(const lanelet_id_extractor_msgs::msg::LaneletInfoUnstamped & src,
                       lanelet_id_extractor_msgs::msg::LaneletInfo & dst)
  {
    dst.lanelet_id = src.lanelet_id;
    dst.is_in_lanelet = src.is_in_lanelet;
    dst.subtype = src.subtype;
    dst.speed_limit = src.speed_limit;
    dst.location = src.location;
    dst.one_way = src.one_way;
    dst.turn_direction = src.turn_direction;
    dst.attributes = src.attributes;
  }

  lanelet_id_extractor_msgs::msg::LaneletInfoUnstamped findLaneletAtPoint(
    const lanelet::BasicPoint2d & point)
  {
    lanelet_id_extractor_msgs::msg::LaneletInfoUnstamped info_msg;

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
      }

    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), "Error finding lanelet: %s", e.what());
    }

    return info_msg;
  }

  void extractLaneletAttributes(
    const lanelet::Lanelet & lanelet,
    lanelet_id_extractor_msgs::msg::LaneletInfoUnstamped & info_msg)
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
  }

  // Parameters
  std::string map_file_path_;
  std::string input_pose_type_;
  double search_radius_;
  double publish_rate_;
  std::string frame_id_;

  // Lanelet map
  lanelet::LaneletMapPtr lanelet_map_;
  bool map_loaded_;

  // ROS2 interfaces - dual publishers
  rclcpp::Publisher<lanelet_id_extractor_msgs::msg::LaneletInfo>::SharedPtr publisher_stamped_;
  rclcpp::Publisher<lanelet_id_extractor_msgs::msg::LaneletInfoUnstamped>::SharedPtr publisher_unstamped_;

  // Subscriptions (only one active based on input_pose_type)
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_sub_pwcs_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_ps_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovariance>::SharedPtr pose_sub_pwc_;
  rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr pose_sub_p_;

  // Rate limiting
  std::chrono::steady_clock::time_point last_publish_time_;
  std::chrono::duration<double> min_publish_interval_;

  // Status tracking
  int64_t last_lanelet_id_;
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
