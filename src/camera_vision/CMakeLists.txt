cmake_minimum_required(VERSION 3.8)
project(camera_vision)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(geometry_msgs REQUIRED)

# --- Noeud 1 : Acquisition Camera ---
add_executable(camera_publisher src/camera_publisher.cpp)
ament_target_dependencies(camera_publisher rclcpp sensor_msgs std_msgs cv_bridge OpenCV)

# --- Noeud 2 : Detection d'obstacles ---
add_executable(obstacle_detector src/obstacle_detector.cpp)
ament_target_dependencies(obstacle_detector rclcpp sensor_msgs std_msgs geometry_msgs cv_bridge OpenCV)

ament_target_dependencies(camera_publisher
  rclcpp
  sensor_msgs
  std_msgs
  cv_bridge
  OpenCV
  geometry_msgs
)

install(TARGETS
  camera_publisher
  obstacle_detector
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
