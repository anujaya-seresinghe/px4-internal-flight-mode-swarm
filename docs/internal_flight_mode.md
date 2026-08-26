# Adding a PX4 custom internal flight mode (custom flight mode swarm is used as an example)

#### Create a flight task according to PX4 documentation
https://docs.px4.io/main/en/concept/flight_tasks

#### Extend the VehicleStatus uORB message 
One of the free flight modes given as (NAVIGATION_STATE_FREE) can be replaced in msg/versioned/VtolVehicleStatus.msg

#### Extend FlightModeManager.cpp
Include your flight mode in start_flight_task function in src/modules/flight_mode_manager/FlightModeManager.cpp
Example:
```cpp
	if (_vehicle_status_sub.get().nav_state == vehicle_status_s::NAVIGATION_STATE_SWARM) {
		found_some_task = true;
		FlightTaskError error = FlightTaskError::InvalidTask;

#if !defined(CONSTRAINED_FLASH)
		error = switchTask(FlightTaskIndex::Swarm);
#endif // !CONSTRAINED_FLASH

		if (error != FlightTaskError::NoError) {
			matching_task_running = false;
			task_failure = true;
		}
	}

```

#### Extend enums.json

Find navigation_mode_t in src/lib/events/enums.json and add your custom flight mode and build so that the enums are updated. 


#### Extend mode requirements
Include your flight mode in getModeRequirements function in src/modules/commander/ModeUtil/mode_requirements.hpp
In this example, the requirements are set as follows:

```cpp
setRequirement(vehicle_status_s::NAVIGATION_STATE_SWARM, flags.mode_req_angular_velocity);
setRequirement(vehicle_status_s::NAVIGATION_STATE_SWARM, flags.mode_req_attitude);
setRequirement(vehicle_status_s::NAVIGATION_STATE_SWARM, flags.mode_req_local_position);
setRequirement(vehicle_status_s::NAVIGATION_STATE_SWARM, flags.mode_req_local_alt);
setRequirement(vehicle_status_s::NAVIGATION_STATE_SWARM, flags.mode_req_prevent_arming);
setRequirement(vehicle_status_s::NAVIGATION_STATE_SWARM, flags.mode_req_wind_and_flight_time_compliance);
```

#### Extend coversions - this maps uOrb flight modes to internal navigation_mode data structures
Extend the function navigation_mode in src/modules/commander/ModeUtil/conversions.hpp
navigation_mode_t enums come from enums.json that is mentioned above. In this example, the function is extneded as follows:

case vehicle_status_s::NAVIGATION_STATE_SWARM: return navigation_mode_t::swarm;


#### Extend commander - control mode (without this, the vehicle's attitude becomes unstable)

Add a case for your flight mode in getVehicleControlMode function in src/modules/commander/ModeUtil/control_mode.cpp 

In this example, this part is done the same as Auto Follow flight mode which just adds a case without doing anything else:
```cpp
case vehicle_status_s::NAVIGATION_STATE_AUTO_FOLLOW_TARGET:
case vehicle_status_s::NAVIGATION_STATE_SWARM:

// Follow Target supports RC adjustment, so disable auto control mode to disable
// the Flight Task from exiting itself when RC stick movement is detected.
case vehicle_status_s::NAVIGATION_STATE_ORBIT:
	vehicle_control_mode.flag_control_manual_enabled = false;
	getControlMode(SetpointType::Trajectory, vehicle_control_mode);
	break;
```

This could be a mistake in the code since, in this way, when auto follow is activated, the code jumps to the code block specified by the orbit flight mode.



## Mavlink interface for flight modes (this maps MAVLink flight mode to uOrb flight mode)
Update flight mode enums in src/modules/commander/px4_custom_mode.h
In this example, swarm flight mode included in PX4_CUSTOM_SUB_MODE_AUTO enum 

update get_px4_custom_mode. For example:
	case vehicle_status_s::NAVIGATION_STATE_SWARM:
		custom_mode.main_mode = PX4_CUSTOM_MAIN_MODE_AUTO;
		custom_mode.sub_mode = PX4_CUSTOM_SUB_MODE_SWARM;
		break;


Extend the switch statement in handle_command in src/modules/commander/commander.cpp 
If you implemented your flight mode as Auto, then search for:

case PX4_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET:
	desired_nav_state = vehicle_status_s::NAVIGATION_STATE_AUTO_FOLLOW_TARGET;
	break;

							
and include your flight mode underneath. 



## Test 





