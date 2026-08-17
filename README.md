# robot

This repository is a beginner-friendly tutorial for building a Raspberry Pi + ESP32 robot that uses ROS 2, SLAM, and eventually Nav2 for waypoint navigation.

The individual pieces are not especially complicated, but getting everything configured correctly can be annoying because the necessary information is scattered across many different tutorials and documentation pages. My goal here is to document the exact setup and commands I use so that I—or anyone following along—can bring the robot up again without having to rediscover everything.

## Current Hardware

My current robot uses:

* Raspberry Pi 5, 4 GB RAM
* RPLIDAR A2M8
* ESP32-S2
* 2x DRV8833 motor drivers
* MPU6050 IMU
* Four DC motors
* 7.4 V 2S LiPo batteries
* Buck converters for regulated power

### Power

The Raspberry Pi requires a solid 5 V supply with enough current available for the Pi itself and its USB peripherals.

If you are powering a Raspberry Pi 5, lidar, and other peripherals from a battery, make sure your buck converter can actually provide sufficient current. A converter that technically outputs 5 V but cannot supply enough current can cause peripheral power limiting, USB problems, brownouts, or instability.

My current setup uses separate power systems:

* One battery/converter for the Raspberry Pi and lidar
* Another battery/converter for the ESP32 and motors

I do not claim this is the optimal electrical architecture. It is simply the configuration I currently use.

Make sure all required grounds are connected appropriately when two systems need to exchange electrical signals.

## A Very Important Recommendation: Use Encoders

If you have not purchased motors yet, buy motors with encoders.

Seriously.

Wheel encoders make obtaining usable odometry dramatically easier.

My current motors do **not** have encoders. This means I cannot directly measure how far each wheel has rotated.

For proper mobile robot navigation, ROS generally expects some estimate of the robot's motion:

```text
map
  ↓
odom
  ↓
base_footprint
  ↓
laser
```

SLAM Toolbox normally provides:

```text
map → odom
```

Your robot's odometry system provides:

```text
odom → base_footprint
```

A static transform defines the physical location of the lidar:

```text
base_footprint → laser
```

Without wheel encoders, producing accurate `odom → base_footprint` is more difficult.

An IMU such as the MPU6050 can measure rotation and acceleration, but an IMU by itself is generally not a good replacement for wheel odometry because integrating acceleration to estimate position accumulates error very quickly.

Later I may experiment with lidar odometry, IMU fusion, or other approaches.

For now, this guide includes a **fake odometry setup** purely for getting SLAM Toolbox running and understanding the ROS TF architecture.

Do not mistake fake odometry for something suitable for actual autonomous navigation.

---

# Software

I am currently using:

```text
Ubuntu 24.04 LTS
ROS 2 Jazzy
```

On my laptop I am specifically running Ubuntu 24.04.3 LTS.

The same ROS 2 distribution should be used across the machines communicating with each other.

The major packages used so far are:

* ROS 2 Jazzy
* RViz2
* SLAM Toolbox
* `sllidar_ros2`
* `tf2_ros`

The Raspberry Pi runs the robot-side ROS nodes.

My laptop runs RViz so I can visualize everything without placing the graphical workload on the Pi.

---

# Network Architecture

The Raspberry Pi and laptop communicate over the local network using ROS 2 DDS.

Both machines must:

1. Be connected to the same network.
2. Use the same `ROS_DOMAIN_ID`.
3. Use compatible ROS 2 middleware settings.

I currently use:

```bash
export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

I run these commands on **both the Raspberry Pi and the laptop**.

Before doing anything ROS-related in a new terminal, source ROS:

```bash
source /opt/ros/jazzy/setup.bash
```

On the Raspberry Pi I also source my ROS workspace:

```bash
source ~/ros2_ws/install/setup.bash
```

So a normal new terminal on the Raspberry Pi begins with:

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

A normal terminal on the laptop begins with:

```bash
source /opt/ros/jazzy/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

Later, I will probably put these commands into `.bashrc` so they do not need to be typed every time.

---

# ROS 2 Workspace

I created a ROS 2 workspace called:

```text
~/ros2_ws
```

A typical ROS 2 workspace looks like:

```text
ros2_ws/
├── build/
├── install/
├── log/
└── src/
```

The source packages are placed inside:

```text
~/ros2_ws/src
```

The workspace is built from:

```bash
cd ~/ros2_ws
colcon build
```

After building:

```bash
source ~/ros2_ws/install/setup.bash
```

If you open a new terminal, you must source the workspace again unless you have added it to `.bashrc`.

---

# Step 1: Start the RPLIDAR

My lidar uses the ROS 2 package:

```text
sllidar_ros2
```

This is important because some tutorials use older package names such as:

```text
rplidar_ros
```

If ROS says:

```text
Package 'rplidar_ros' not found
```

but your workspace contains:

```text
sllidar_ros2
```

then you are simply using the newer package.

First source everything:

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

You can inspect the installed launch files with:

```bash
ls ~/ros2_ws/install/sllidar_ros2/share/sllidar_ros2/launch
```

For my RPLIDAR A2M8, launch the lidar with the appropriate A2M8 launch file.

For example:

```bash
ros2 launch sllidar_ros2 sllidar_a2m8_launch.py
```

Leave this terminal running.

---

# Step 2: Verify `/scan`

Open another terminal on the Raspberry Pi.

Source ROS again:

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

List topics:

```bash
ros2 topic list
```

You should see:

```text
/scan
```

Check the lidar publishing frequency:

```bash
ros2 topic hz /scan
```

My RPLIDAR A2M8 publishes at roughly:

```text
~10.8 Hz
```

You can also inspect one scan:

```bash
ros2 topic echo /scan --once
```

The header should contain something similar to:

```yaml
header:
  frame_id: laser
```

Important:

```text
frame_id: laser
```

inside the LaserScan message does **not** automatically create a TF frame called `laser`.

A TF broadcaster must separately define where the `laser` frame is located relative to the robot.

---

# Step 3: Verify the Laptop Can See the Pi

On the laptop:

```bash
source /opt/ros/jazzy/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

Then:

```bash
ros2 topic list
```

If networking is working correctly, `/scan` should appear on the laptop even though the lidar is physically connected to the Raspberry Pi.

You can confirm:

```bash
ros2 topic hz /scan
```

If the laptop receives lidar scans, ROS 2 communication between the computers is working.

---

# Step 4: Start RViz2

On the laptop:

```bash
rviz2
```

For the first lidar test, set:

```text
Global Options
    Fixed Frame = laser
```

Then:

```text
Add
→ LaserScan
```

Set:

```text
Topic = /scan
```

You should now see lidar points appearing in RViz.

### RViz Camera Controls

Useful controls:

```text
Left drag              Rotate
Middle drag            Pan
Shift + left drag      Pan
Mouse wheel            Zoom
```

---

# Step 5: Create the Lidar TF

SLAM Toolbox needs to know where the lidar is mounted relative to the robot.

The TF relationship is:

```text
base_footprint → laser
```

Eventually this transform should contain the **actual measured physical offset** of the lidar from the robot's coordinate origin.

For initial testing, I use a zero transform:

```bash
ros2 run tf2_ros static_transform_publisher \
  --x 0 \
  --y 0 \
  --z 0 \
  --roll 0 \
  --pitch 0 \
  --yaw 0 \
  --frame-id base_footprint \
  --child-frame-id laser
```

Leave this terminal running.

Verify:

```bash
ros2 run tf2_ros tf2_echo base_footprint laser
```

You should see a valid transform continuously printed.

---

# Step 6: Odometry

SLAM Toolbox expects an odometry frame.

The relevant TF hierarchy is:

```text
odom
  ↓
base_footprint
  ↓
laser
```

A proper robot would normally calculate:

```text
odom → base_footprint
```

using wheel encoders and potentially an IMU.

Because my motors currently have no encoders, I initially use fake odometry while developing the rest of the ROS stack.

## Static Fake Odometry

For the simplest possible test:

```bash
ros2 run tf2_ros static_transform_publisher \
  --x 0 \
  --y 0 \
  --z 0 \
  --roll 0 \
  --pitch 0 \
  --yaw 0 \
  --frame-id odom \
  --child-frame-id base_footprint
```

This creates:

```text
odom → base_footprint
```

However, because this transform never changes, ROS believes the robot never moves.

This may allow SLAM Toolbox to initialize, but it is not useful for actually mapping while moving the robot.

Verify:

```bash
ros2 run tf2_ros tf2_echo odom base_footprint
```

---

# Step 7: Start SLAM Toolbox

Open another terminal on the Raspberry Pi.

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

Launch SLAM Toolbox:

```bash
ros2 launch slam_toolbox online_async_launch.py
```

Leave it running.

Check that the node exists:

```bash
ros2 node list
```

You should see:

```text
/slam_toolbox
```

Check map topics:

```bash
ros2 topic list | grep map
```

You should see:

```text
/map
/map_metadata
```

---

# Step 8: Display the Map in RViz

On the laptop, change:

```text
Global Options
    Fixed Frame = map
```

Then:

```text
Add
→ Map
```

Set:

```text
Topic = /map
```

If everything is working, RViz should display an occupancy grid.

The final TF hierarchy should look approximately like:

```text
map
  ↓
odom
  ↓
base_footprint
  ↓
laser
```

The publishers responsible for these transforms are:

```text
map → odom
    SLAM Toolbox

odom → base_footprint
    Robot odometry system

base_footprint → laser
    Static transform describing lidar mounting position
```

---

# Debugging: "Frame map does not exist"

If RViz reports:

```text
Frame [map] does not exist
```

check whether SLAM Toolbox is actually running:

```bash
ros2 node list
```

Then check:

```bash
ros2 topic list | grep map
```

Next inspect the TF chain.

Check:

```bash
ros2 run tf2_ros tf2_echo odom base_footprint
```

Then:

```bash
ros2 run tf2_ros tf2_echo base_footprint laser
```

Finally:

```bash
ros2 run tf2_ros tf2_echo odom laser
```

If the last command works, ROS can transform lidar measurements into the odometry coordinate system.

Once SLAM Toolbox begins processing scans, this should also work:

```bash
ros2 run tf2_ros tf2_echo map odom
```

---

# Debugging: `/scan` Says `laser`, But TF Says `laser` Does Not Exist

This confused me initially.

Running:

```bash
ros2 topic echo /scan --once
```

may show:

```yaml
frame_id: laser
```

while:

```bash
ros2 run tf2_ros tf2_echo odom laser
```

reports:

```text
Invalid frame ID "laser"
```

These do not contradict each other.

The LaserScan message is simply **claiming that its measurements belong to a coordinate frame named `laser`**.

ROS does not know where that frame physically exists until something broadcasts a TF involving it.

Create:

```text
base_footprint → laser
```

using:

```bash
ros2 run tf2_ros static_transform_publisher \
  --x 0 \
  --y 0 \
  --z 0 \
  --roll 0 \
  --pitch 0 \
  --yaw 0 \
  --frame-id base_footprint \
  --child-frame-id laser
```

---

# Fake Moving Odometry

A static `odom → base_footprint` transform causes SLAM to believe the robot is permanently stationary.

If I simply want to test SLAM and make the map continuously attempt to update, I can publish deliberately fake moving odometry.

Create:

```bash
nano ~/fake_odom.py
```

Paste:

```python
#!/usr/bin/env python3

import math

import rclpy
from rclpy.node import Node

from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from tf2_ros import TransformBroadcaster


class FakeOdom(Node):

    def __init__(self):
        super().__init__('fake_odom')

        self.tf_broadcaster = TransformBroadcaster(self)

        self.odom_pub = self.create_publisher(
            Odometry,
            '/odom',
            10
        )

        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0

        self.linear_velocity = 0.05
        self.angular_velocity = 0.03

        self.dt = 0.05

        self.timer = self.create_timer(
            self.dt,
            self.update
        )

    def update(self):

        self.theta += self.angular_velocity * self.dt

        self.x += (
            self.linear_velocity
            * math.cos(self.theta)
            * self.dt
        )

        self.y += (
            self.linear_velocity
            * math.sin(self.theta)
            * self.dt
        )

        now = self.get_clock().now().to_msg()

        qz = math.sin(self.theta / 2.0)
        qw = math.cos(self.theta / 2.0)

        transform = TransformStamped()

        transform.header.stamp = now
        transform.header.frame_id = 'odom'
        transform.child_frame_id = 'base_footprint'

        transform.transform.translation.x = self.x
        transform.transform.translation.y = self.y
        transform.transform.translation.z = 0.0

        transform.transform.rotation.x = 0.0
        transform.transform.rotation.y = 0.0
        transform.transform.rotation.z = qz
        transform.transform.rotation.w = qw

        self.tf_broadcaster.sendTransform(transform)

        odom = Odometry()

        odom.header.stamp = now
        odom.header.frame_id = 'odom'
        odom.child_frame_id = 'base_footprint'

        odom.pose.pose.position.x = self.x
        odom.pose.pose.position.y = self.y

        odom.pose.pose.orientation.z = qz
        odom.pose.pose.orientation.w = qw

        odom.twist.twist.linear.x = self.linear_velocity
        odom.twist.twist.angular.z = self.angular_velocity

        self.odom_pub.publish(odom)


def main():

    rclpy.init()

    node = FakeOdom()

    try:
        rclpy.spin(node)

    except KeyboardInterrupt:
        pass

    node.destroy_node()

    rclpy.shutdown()


if __name__ == '__main__':
    main()
```

Run:

```bash
python3 ~/fake_odom.py
```

Verify:

```bash
ros2 run tf2_ros tf2_echo odom base_footprint
```

The translation and rotation values should now continuously change.

### IMPORTANT

Do **not** simultaneously run:

```text
static odom → base_footprint
```

and:

```text
fake_odom.py odom → base_footprint
```

Both nodes would be attempting to publish the same TF relationship.

Stop the static odometry publisher before starting `fake_odom.py`.

This moving fake odometry is intentionally wrong. The resulting SLAM map may:

* smear
* warp
* duplicate walls
* rotate incorrectly
* drift badly

That is expected.

Its purpose is only to test the ROS pipeline:

```text
Lidar
 ↓
/scan
 ↓
TF
 ↓
SLAM Toolbox
 ↓
/map
 ↓
RViz
```

---

# Current Startup Procedure

Until I automate this with launch files, the complete manual startup sequence is approximately:

## Raspberry Pi Terminal 1 — Lidar

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

ros2 launch sllidar_ros2 sllidar_a2m8_launch.py
```

## Raspberry Pi Terminal 2 — Lidar TF

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

ros2 run tf2_ros static_transform_publisher \
  --x 0 \
  --y 0 \
  --z 0 \
  --roll 0 \
  --pitch 0 \
  --yaw 0 \
  --frame-id base_footprint \
  --child-frame-id laser
```

## Raspberry Pi Terminal 3 — Fake Odometry

For static testing:

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

ros2 run tf2_ros static_transform_publisher \
  --x 0 \
  --y 0 \
  --z 0 \
  --roll 0 \
  --pitch 0 \
  --yaw 0 \
  --frame-id odom \
  --child-frame-id base_footprint
```

OR, for deliberately moving fake odometry:

```bash
source /opt/ros/jazzy/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

python3 ~/fake_odom.py
```

Use **one or the other**, not both.

## Raspberry Pi Terminal 4 — SLAM Toolbox

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/install/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

ros2 launch slam_toolbox online_async_launch.py
```

## Laptop — RViz

```bash
source /opt/ros/jazzy/setup.bash

export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

rviz2
```

Then configure:

```text
Fixed Frame = map

LaserScan:
    Topic = /scan

Map:
    Topic = /map
```

At this point the laptop should be displaying both the lidar scan and the SLAM occupancy grid generated by the Raspberry Pi.

---

# Eventually: Replace All of This With One Launch File

Opening four terminals and manually starting every ROS node is useful while learning because it makes the architecture obvious.

It is not how I want to operate the finished robot.

Eventually the Raspberry Pi should have one launch command that starts:

```text
RPLIDAR driver
        ↓
base_footprint → laser TF
        ↓
ESP32 interface
        ↓
odometry
        ↓
robot_localization / sensor fusion
        ↓
SLAM Toolbox or Nav2
```

The final goal should be something approximately like:

```bash
ros2 launch robot_bringup robot.launch.py
```

and the entire robot comes alive.

That will be added once the individual components are working reliably.




