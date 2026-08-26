#!/usr/bin/env python3

from pymavlink import mavutil
import time

# Connect to PX4
# SITL UDP:
master = mavutil.mavlink_connection('udp:127.0.0.1:14030')

# Real vehicle example:
# master = mavutil.mavlink_connection('/dev/ttyACM0', baud=57600)

print("Waiting for heartbeat...")
master.wait_heartbeat()

print(f"Connected to system {master.target_system}, component {master.target_component}")

# PX4 custom mode values
base_mode = mavutil.mavlink.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED  # 1
custom_main_mode = 4   # PX4_CUSTOM_MAIN_MODE_AUTO
custom_sub_mode = 11    # PX4_CUSTOM_SUB_MODE_AUTO_SWARM

print("Sending AUTO_SWARM mode command...")

master.mav.command_long_send(
    master.target_system,
    master.target_component,
    mavutil.mavlink.MAV_CMD_DO_SET_MODE,
    0,                  # confirmation
    base_mode,
    custom_main_mode,
    custom_sub_mode,
    0,
    0,
    0,
    0
)

time.sleep(2)

# Check response
msg = master.recv_match(type='COMMAND_ACK', blocking=True, timeout=3)

if msg:
    print(msg)
else:
    print("No ACK received")
