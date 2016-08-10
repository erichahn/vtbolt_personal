#include <stdint.h>
#include "an_packet_protocol.h"
#include "spatial_packets.h"


#include <ros/ros.h>
#include <serial/serial.h>
#include <std_msgs/String.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Float64.h>
#include <sensor_msgs/Imu.h>
#include <an_ros_wrapper/Spatial.h>

#define RADIANS_TO_DEGREES (180.0/M_PI)
#define MESSAGE_BUFFER_SIZE (10)

serial::Serial ser;

void write_callback(const std_msgs::String::ConstPtr& msg) {
    ROS_INFO_STREAM("Writing to serial port" << msg->data);
    ser.write(msg->data);
}

int main (int argc, char** argv) {
    ros::init(argc, argv, "spatial_node");
    ros::NodeHandle nh;

    ros::Publisher spatial_ins = nh.advertise<an_ros_wrapper::Spatial>("spatial", MESSAGE_BUFFER_SIZE);
    // ros::Subscriber write_sub = nh.subscribe("write", 1000, write_callback);
    // ros::Publisher lat_pub = nh.advertise<std_msgs::Float64>("spatial/lat", MESSAGE_BUFFER_SIZE);
    // ros::Publisher lon_pub = nh.advertise<std_msgs::Float64>("spatial/long", MESSAGE_BUFFER_SIZE);
    // ros::Publisher height_pub = nh.advertise<std_msgs::Float64>("spatial/height", MESSAGE_BUFFER_SIZE);
    // ros::Publisher pressure_pub = nh.advertise<std_msgs::Float64>("spatial/pressure", MESSAGE_BUFFER_SIZE);
    // ros::Publisher imu_pub = nh.advertise<sensor_msgs::Imu>("spatial/imu_raw", MESSAGE_BUFFER_SIZE);
    // ros::Publisher imu_info = nh.advertise<std_msgs::String>("spatial/info", MESSAGE_BUFFER_SIZE);

    // std_msgs::Float64 lat;
    // std_msgs::Float64 lon;
    // std_msgs::Float64 height;
    // std_msgs::Float64 pressure;

    // std_msgs::String info;

    // sensor_msgs::Imu spatial;

    an_ros_wrapper::Spatial spatial;
    try
    {
        ser.setPort("/dev/ttyUSB0");
        ser.setBaudrate(921600);
        // serial::Timeout to = serial::Timeout::simpleTimeout(1000);
        // ser.setTimeout(to);
        ser.open();
    }
    catch (serial::IOException& e)
    {
        ROS_ERROR_STREAM("Unable to open port ");
        return -1;
    }

    if (ser.isOpen()) {
        ROS_INFO_STREAM("Serial Port initialized");
    } else {
        return -1;
    }

    an_decoder_t an_decoder;
    an_packet_t *an_packet;

    system_state_packet_t system_state_packet;
    raw_sensors_packet_t raw_sensors_packet;

    an_decoder_initialise(&an_decoder);

    int bytes_received;

    ros::Rate loop_rate(1000);
    ROS_INFO_STREAM("Reading from serial port");
    while (ros::ok()) {

        if ((bytes_received = ser.read(an_decoder_pointer(&an_decoder), an_decoder_size(&an_decoder))) > 0)
        {
            spatial.header.stamp = ros::Time::now();
            /* increment the decode buffer length by the number of bytes received */
            an_decoder_increment(&an_decoder, bytes_received);

            /* decode all the packets in the buffer */
            while ((an_packet = an_packet_decode(&an_decoder)) != NULL)
            {
                if (an_packet->id == packet_id_system_state) /* system state packet */
                {
                    /* copy all the binary data into the typedef struct for the packet */
                    /* this allows easy access to all the different values             */
                    if (decode_system_state_packet(&system_state_packet, an_packet) == 0)
                    {
                        spatial.navsatfix.latitude = system_state_packet.latitude * RADIANS_TO_DEGREES;
                        spatial.navsatfix.longitude = system_state_packet.longitude * RADIANS_TO_DEGREES;
                        spatial.navsatfix.altitude = system_state_packet.height;
                    }
                }
                else if (an_packet->id == packet_id_raw_sensors) /* raw sensors packet */
                {
                    /* copy all the binary data into the typedef struct for the packet */
                    /* this allows easy access to all the different values             */
                    if (decode_raw_sensors_packet(&raw_sensors_packet, an_packet) == 0)
                    {
                        spatial.imu.linear_acceleration.x = raw_sensors_packet.accelerometers[0];
                        spatial.imu.linear_acceleration.y = raw_sensors_packet.accelerometers[1];
                        spatial.imu.linear_acceleration.z = raw_sensors_packet.accelerometers[2];

                        spatial.imu.angular_velocity.x = raw_sensors_packet.gyroscopes[0] * RADIANS_TO_DEGREES;
                        spatial.imu.angular_velocity.y = raw_sensors_packet.gyroscopes[1] * RADIANS_TO_DEGREES;
                        spatial.imu.angular_velocity.z = raw_sensors_packet.gyroscopes[2] * RADIANS_TO_DEGREES;

                        spatial.pressure.fluid_pressure = raw_sensors_packet.pressure;
                    }
                }
                else
                {
                    // info.data = "Packet ID ";
                    // info.data += an_packet->id;
                    // info.data += " of Length ";
                    // info.data += an_packet->length;
                    // imu_info.publish(info);
                    // printf("Packet ID %u of Length %u\n", an_packet->id, an_packet->length);
                }

                /* Ensure that you free the an_packet when your done with it or you will leak memory */
                an_packet_free(&an_packet);
                spatial_ins.publish(spatial);
            }
            ros::spinOnce();
            loop_rate.sleep();
        }
        // if (ser.available()) {
        //     ROS_INFO_STREAM("Reading from serial port");
        //     std_msgs::String result;
        //     result.data = ser.read(ser.available());
        //     ROS_INFO_STREAM("Read: " << result.data);
        //     read_pub.publish(result);
        // }
        // loop_rate.sleep();

    }
}