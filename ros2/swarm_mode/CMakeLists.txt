cmake_minimum_required(VERSION 3.8)
project(swarm_mode)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

find_package(ament_cmake REQUIRED)
find_package(eigen3_cmake_module REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(px4_msgs REQUIRED)
find_package(px4_ros2_cpp REQUIRED)

# MAVLink C headers with the swarm messages (SWARM_MANAGEMENT 601, SWARM_NODE 602).
# Defaults to the repository's libraries/ folder (ros2/swarm_mode -> ../../libraries).
set(SWARM_MAVLINK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries"
  CACHE PATH "Directory that contains mavlink/common/mavlink.h")
if(NOT EXISTS "${SWARM_MAVLINK_INCLUDE_DIR}/mavlink/common/mavlink.h")
  message(FATAL_ERROR "mavlink/common/mavlink.h not found in ${SWARM_MAVLINK_INCLUDE_DIR}")
endif()

add_executable(swarm_mode_node
  src/main.cpp
  src/swarm_mode.cpp
)
target_include_directories(swarm_mode_node PRIVATE include)
target_include_directories(swarm_mode_node SYSTEM PRIVATE ${SWARM_MAVLINK_INCLUDE_DIR})
target_link_libraries(swarm_mode_node Eigen3::Eigen px4_ros2_cpp::px4_ros2_cpp)
ament_target_dependencies(swarm_mode_node rclcpp px4_msgs)

install(TARGETS swarm_mode_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
