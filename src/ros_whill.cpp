/*
MIT License

Copyright (c) 2018 WHILL inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <string.h>
#include <stdint.h>
#include <vector>

#include "ros/ros.h"
#include "sensor_msgs/Joy.h"
#include "sensor_msgs/JointState.h"
#include "sensor_msgs/Imu.h"
#include "sensor_msgs/BatteryState.h"
#include "nav_msgs/Odometry.h"

#include "whill/WHILL.h"   
#include "serial/serial.h"  // wjwood/Serial (ros-melodic-serial)

// #include "./includes/subscriber.hpp"
// #include "./includes/services.hpp"

WHILL *whill = nullptr;

//
// ROS Objects
//

// Publishers
ros::Publisher joystick_state_publisher;
ros::Publisher jointstate_publisher;
ros::Publisher imu_publisher;
ros::Publisher battery_state_publisher;
ros::Publisher odom_publisher;


// 
// ROS Callbacks
//
void ros_joystick_callback(const sensor_msgs::Joy::ConstPtr &joy)
{
    // Transform [-1.0,1.0] to [-100,100]
    int joy_x = -joy->axes[0] * 100.0f;
    int joy_y = joy->axes[1] * 100.0f;

    // value check
    if (joy_y < -100)
        joy_y = -100;
    if (joy_y > 100)
        joy_y = 100;
    if (joy_x < -100)
        joy_x = -100;
    if (joy_x > 100)
        joy_x = 100;

    if (whill)
    {
        whill->setJoystick(joy_x, joy_y);
    }
}

void ros_cmd_vel_callback(const geometry_msgs::Twist::ConstPtr &cmd_vel)
{
    if (whill)
    {
        whill->setSpeed(cmd_vel->linear.x, cmd_vel->angular.z);
    }
}

//
//  UART Interface
//
serial::Serial *ser = nullptr;

int serialRead(std::vector<uint8_t> &data)
{
    if (ser && ser->isOpen())
    {
        return ser->read(data, 30); // How many bytes read in one time.
    }
    return 0;
}

int serialWrite(std::vector<uint8_t> &data)
{
    if (ser && ser->isOpen())
    {
        return ser->write(data);
    }
    return 0;
}

//
// WHILL
//
void whill_callback_data1(WHILL *caller)
{
    // This function is called when receive Joy/Accelerometer/Gyro,etc.
    ROS_INFO("Updated");
    ROS_INFO("%d",caller->joy.x);
    ROS_INFO("%d",caller->joy.y);
    ROS_INFO("interval:%d",caller->_interval);
}

void whill_callback_powered_on(WHILL *caller)
{
    // This function is called when powered on via setPower()
    ROS_INFO("power_on");
}


//
// Main
//
int main(int argc, char **argv)
{
    // ROS setup
    ros::init(argc, argv, "whill");
    ros::NodeHandle nh("~");

    std::string serialport;
    nh.param<std::string>("serialport", serialport, "/dev/ttyUSB0");

    bool activate_experimental;
    nh.param<bool>("activate_experimental", activate_experimental, false);

    // Services
    //set_power_service_service = nh.advertiseService("power/on", set_power_service_callback);
    //clear_odom_service        = nh.advertiseService("odom/clear", &clearOdom);

    // Subscriber
    ros::Subscriber joystick_subscriber = nh.subscribe("controller/joy", 100, ros_joystick_callback);
    ros::Subscriber twist_subscriber = nh.subscribe("controller/cmd_vel", 100, ros_cmd_vel_callback);

    // // Publishers
    joystick_state_publisher = nh.advertise<sensor_msgs::Joy>("states/joy", 100);
    jointstate_publisher = nh.advertise<sensor_msgs::JointState>("states/jointState", 100);
    imu_publisher = nh.advertise<sensor_msgs::Imu>("states/imu", 100);
    battery_state_publisher = nh.advertise<sensor_msgs::BatteryState>("states/batteryState", 100);
    odom_publisher = nh.advertise<nav_msgs::Odometry>("odom", 100);
 
    if (activate_experimental)
    {
        //Services

        //Subscribers
        ros::Subscriber control_cmd_vel_subscriber = nh.subscribe("controller/cmd_vel", 100, ros_cmd_vel_callback);

        // Publishers
    }



    // Node Param
    int send_interval = 10;
    nh.getParam("send_interval", send_interval);
    if (send_interval < 10)
    {
        ROS_WARN("Too short interval. Set interval > 10");
        nh.setParam("send_interval", 10);
        send_interval = 10;
    }
    ROS_INFO("param: send_interval=%d", send_interval);

    std::string port = "/dev/tty.whill";
    unsigned long baud = 38400;

    serial::Timeout timeout = serial::Timeout::simpleTimeout(0);
    timeout.write_timeout_multiplier = 5;

    ser = new serial::Serial(port, baud, timeout);
    ser->flush();

    whill = new WHILL(serialRead, serialWrite);
    whill->register_callback(whill_callback_data1, WHILL::EVENT::CALLBACK_DATA1);
    whill->register_callback(whill_callback_powered_on, WHILL::EVENT::CALLBACK_POWER_ON);
    whill->begin(10); // ms



    ros::AsyncSpinner spinner(1);
    spinner.start();
    ros::Rate rate(100);

    while (ros::ok())
    {
        whill->refresh();
        //whill->setSpeed(0.0f,0.5f);
        rate.sleep();
    }

    spinner.stop();

    return 0;
}