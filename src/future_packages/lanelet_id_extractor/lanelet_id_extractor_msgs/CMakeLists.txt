cmake_minimum_required(VERSION 3.8)
project(lanelet_id_extractor_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)

# Generate messages
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/LaneletInfo.msg"
  "msg/LaneletInfoUnstamped.msg"
  DEPENDENCIES std_msgs diagnostic_msgs
)

ament_package()
