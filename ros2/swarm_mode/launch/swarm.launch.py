"""Start one Swarm external-mode node per PX4 SITL instance.

    ros2 launch swarm_mode swarm.launch.py drones:=5
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def nodes(context):
    count = int(LaunchConfiguration("drones").perform(context))
    external1 = int(LaunchConfiguration("external1_sub_mode").perform(context))
    return [
        Node(
            package="swarm_mode",
            executable="swarm_mode_node",
            name=f"swarm_mode_{i}",
            output="screen",
            parameters=[{"px4_instance": i, "external1_sub_mode": external1}],
        )
        for i in range(count)
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("drones", default_value="5", description="Number of PX4 instances (0..N-1)"),
        DeclareLaunchArgument("external1_sub_mode", default_value="12",
                              description="PX4_CUSTOM_SUB_MODE_EXTERNAL1 of the PX4 build"),
        OpaqueFunction(function=nodes),
    ])
