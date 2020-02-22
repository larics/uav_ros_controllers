 /*
 * @author Robert Milijas
 * @version 0.1
 * @date December 2019
 *
 */

#ifndef BALLOON_STATE_MACHINE_H
#define BALLOON_STATE_MACHINE_H

#include <ros/ros.h>
#include <nav_msgs/Odometry.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Int32.h>
#include <std_msgs/String.h>
#include <std_srvs/Empty.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/PoseStamped.h>
#include <dynamic_reconfigure/server.h>
#include <std_srvs/Empty.h>
#include <std_srvs/SetBool.h>

#include "uav_ros_control/reference/Global2Local.h"
#include <uav_ros_control/filters/NonlinearFilters.h>
#include <visualization_msgs/MarkerArray.h>
#include <visualization_msgs/Marker.h>
#include <queue>

#include <trajectory_msgs/MultiDOFJointTrajectoryPoint.h>


 namespace uav_reference
{


enum State {
    TAKE_OFF,
    GETNEXTPOINT,
    GOTOPOINT,
    POINTREACHED,
    SPINNING,
    POPPING,
    RETURN_TO_TRAJECTORY,
    LAND,
    DONE
};

class BalloonStateMachine
{

public:

BalloonStateMachine(ros::NodeHandle& nh) : _globalToLocal(nh)
{
    // Define Publishers
    _pubState = nh.advertise<std_msgs::Int32>("balloon_sm/state", 1);
    _pubGoal = nh.advertise<geometry_msgs::PoseStamped>("balloon_sm/goal", 1);

    // Define Subscribers
    _subOdom = nh.subscribe("odometry", 1, &BalloonStateMachine::odomCb, this);
    _subPointReached = nh.subscribe("point_reached", 1, &BalloonStateMachine::pointReachedCallback, this);
    _pubGoalsMarker = nh.advertise<visualization_msgs::MarkerArray>("balloon_sm/goals_marker", 20);

    _carrot_subscriber = nh.subscribe("carrot/trajectory", 1, &BalloonStateMachine::carrotCb, this);

    ros::Duration(1.0).sleep();
    ros::spinOnce();

    _global_z = _currentReference.z;

    for (auto glob_waypoint : _global_waypoints){

        AddLocalWaypoint(glob_waypoint.first, glob_waypoint.second, 0);
        // FullRotationArroundZAxis(glob_waypoint.first, glob_waypoint.second, 0);
    }

    // Set initial state
    _currentState = State::GETNEXTPOINT;

    // publishGeneratedWaypoints();

}

~BalloonStateMachine()
{}

void AddLocalWaypoint(double latitude, double longitude, double yaw_requested){
    // Za svaki lat,long dodaj točku za goto + tri točke za spin

    Eigen::Vector3d local = _globalToLocal.toLocal(latitude, longitude, _global_alt);
    auto odom_q= _currOdom.pose.pose.orientation;



    // calculate yaw
    double yaw = yaw_requested; // - util::calculateYaw(odom_q.x, odom_q.y, odom_q.z, odom_q.w);

    // calculate yaw from curent reference and next point


    if (_prva){
        _prva = false;
        _yaw = atan2(local.y() - _currentReference.y, local.x() - _currentReference.x);
    }
    else {
        _yaw = atan2(local.y() - _last_waypoint_y, local.x() - _last_waypoint_x);
    }
    yaw = _yaw;

    geometry_msgs::PoseStamped tmp_pose;
    tmp_pose.pose.position.x = local.x();
    tmp_pose.pose.position.y = local.y();
    tmp_pose.pose.position.z = _global_z;

    tf2::Quaternion q;
    q.setRPY(0,0,yaw);
    tmp_pose.pose.orientation.w = q.w();
    tmp_pose.pose.orientation.x = q.x();
    tmp_pose.pose.orientation.y = q.y();
    tmp_pose.pose.orientation.z = q.z();
    _local_waypoints.push(tmp_pose);
    ROS_WARN("x: %f, y: %f, yaw: %f\n", tmp_pose.pose.position.x, tmp_pose.pose.position.y,
             util::calculateYaw(q.x(), q.y(), q.z(), q.w()));

    _last_waypoint_x = local.x();
    _last_waypoint_y = local.y();
}

    void carrotCb(trajectory_msgs::MultiDOFJointTrajectoryPoint trajectory){
    _currentReference = trajectory.transforms[0].translation;
}

// void publishGeneratedWaypoints()
// {
//         // std::vector<geometry_msgs::PoseStamped> m_vectorOfPointsOutdoor)
//         visualization_msgs::MarkerArray Markerarr;
//         visualization_msgs::Marker Marker;
//         Markerarr.markers.resize(_local_waypoints.size());
//         std::queue<geometry_msgs::PoseStamped> tmp_queue;
//
//         ROS_INFO("Create markerarray");
//         int i = 0;
//         while (!_local_waypoints.empty())
//         {
//             geometry_msgs::PoseStamped tmp_pose = _local_waypoints.front();
//             tmp_queue.push(tmp_pose);
//
//
//             Marker.header.frame_id = "world";
//             Marker.header.stamp = ros::Time::now();
//             Marker.id = i++;
//             Marker.ns = "goals";
//             Marker.type = visualization_msgs::Marker::SPHERE;
//             Marker.pose.position.x = tmp_pose.pose.position.x;
//             Marker.pose.position.y = tmp_pose.pose.position.y;
//             Marker.pose.position.z = tmp_pose.pose.position.z;
//             Marker.pose.orientation.x = 0;
//             Marker.pose.orientation.y = 0;
//             Marker.pose.orientation.z = 0;
//             Marker.pose.orientation.w = 1;
//
//             Marker.scale.x = 0.25;
//             Marker.scale.y = 0.25;
//             Marker.scale.z = 0.1;
//             Marker.color.a = 1.0;
//             Marker.color.r = 0.0;
//             Marker.color.g = 1.0;
//             Marker.color.b = 0.0;
//             Marker.lifetime = ros::Duration(360);
//
//             Markerarr.markers.push_back(Marker);
//
//             ROS_INFO("Finally publishing markers!");
//
//             _pubGoalsMarker.publish(Markerarr);
//
//             _local_waypoints.pop();
//         }
//
//         // Refil points
// }

void FullRotationArroundZAxis(double x, double y){

    geometry_msgs::PoseStamped tmp_pose = geometry_msgs::PoseStamped() ;
    tmp_pose.pose.position.x = x;
    tmp_pose.pose.position.y = y;
    tmp_pose.pose.position.z = _global_z;

    tf2::Quaternion q;
    q.setRPY(0, 0, 2);
    q.normalize();
    tmp_pose.pose.orientation.w = q.w();
    tmp_pose.pose.orientation.x = q.x();
    tmp_pose.pose.orientation.y = q.y();
    tmp_pose.pose.orientation.z = q.z();
    _spinning_waypoints.push(tmp_pose);

    ROS_WARN("x: %f, y: %f, yaw: %f\n", tmp_pose.pose.position.x, tmp_pose.pose.position.y,
            util::calculateYaw(q.x(), q.y(),q.z(), q.w()));

    tmp_pose = geometry_msgs::PoseStamped() ;
    tmp_pose.pose.position.x = x;
    tmp_pose.pose.position.y = y;
    // tmp_pose.pose.position.z = local.z();
    tmp_pose.pose.position.z = _global_z;
    q.setRPY(0, 0, 4);
    q.normalize();
    tmp_pose.pose.orientation.w = q.w();
    tmp_pose.pose.orientation.x = q.x();
    tmp_pose.pose.orientation.y = q.y();
    tmp_pose.pose.orientation.z = q.z();
    _spinning_waypoints.push(tmp_pose);

            util::calculateYaw(q.x(), q.y(),q.z(), q.w()));

    tmp_pose = geometry_msgs::PoseStamped() ;
    tmp_pose.pose.position.x = x;
    tmp_pose.pose.position.y = y;
    // tmp_pose.pose.position.z = local.z();
    tmp_pose.pose.position.z = _global_z;
    q.setRPY(0, 0, 6); // intentionally don't use eigen_pi. Not a full rotation is needed.
    q.normalize();
    tmp_pose.pose.orientation.w = q.w();
    tmp_pose.pose.orientation.x = q.x();
    tmp_pose.pose.orientation.y = q.y();
    tmp_pose.pose.orientation.z = q.z();
    _spinning_waypoints.push(tmp_pose);
    ROS_WARN("x: %f, y: %f, yaw: %f\n", tmp_pose.pose.position.x, tmp_pose.pose.position.y,
            util::calculateYaw(q.x(), q.y(), q.z(), q.w()));
    ROS_ERROR("POINT_GENERATED \n");

}


void pointReachedCallback(const std_msgs::Bool msg) {
    _isPointReached = msg.data;
    ROS_INFO("Current point is reached.");
}

void odomCb(const nav_msgs::Odometry msg){
    _currOdom = msg;
}

void publishCurrGoal(geometry_msgs::PoseStamped point)
{
    //ROS_INFO("Inside publish current goal!");
    // geometry_msgs::PoseStamped _goalToPub;
    point.header.stamp = ros::Time::now();
    _isPointReached = false;

    ros::Duration(0.2).sleep();
    ROS_WARN("Pub goal\n");
    _pubGoal.publish(point);
    auto q = point.pose.orientation;
    ROS_WARN("x: %f, y: %f, yaw: %f\n", point.pose.position.x, point.pose.position.y,
            util::calculateYaw(q.x, q.y, q.z, q.w));
}

double distance(geometry_msgs::Point point, Eigen::Vector3d vector){
    return sqrt(
            (point.x - vector.x()) * (point.x - vector.x())+
            (point.y - vector.y()) * (point.y - vector.y())+
            (point.z - vector.z()) * (point.z - vector.z())
            );
}

void stateAction(){
    switch(_currentState){

        case State::GETNEXTPOINT:

            ROS_WARN("[Balloon_sm] Get next point\n");

            if (_spinning){
                publishCurrGoal(_spinning_waypoints.front());
            }
            else {
                publishCurrGoal(_local_waypoints.front());
                _currentState = State::GOTOPOINT;

            }

            break;

        case State::GOTOPOINT:


            if (_isPointReached){
                _currentState = State::POINTREACHED;

                _spinning = true;
            }

           break;

        case State::POINTREACHED:

            ROS_WARN("[Balloon_sm] Point reached\n");

            if (_spinning){
                _spinning_waypoints.pop();
                if(_spinning_waypoints.empty()){
                    _spinning = false;
                }
                else break;
            }

            _local_waypoints.pop();
            if (!_local_waypoints.empty()) {
                _currentState = State::GETNEXTPOINT;
            } else {
                _currentState = State::LAND;
            }


        case State::POPPING :

            ROS_WARN("[Balloon_sm] Popping\n");

            // Generate trajectory towards the balloon

            // The trajectory should wait at the balloon and then move up - down, and wait
            break;
        case State::RETURN_TO_TRAJECTORY:

            ROS_WARN("[Balloon_sm] Return to trajectory\n");
            // Generate trajectory towards the last search waypoint and execute it.
            break;

        case State::LAND:

            ROS_WARN("[Balloon_sm] Land\n");
            _currentState = State::DONE;
            break;
    }


}


void run()
{
    ros::Rate loopRate(_rate);
    double dt = 1.0 / _rate;
	while (ros::ok())
	{
		ros::spinOnce();
		stateAction();
		_intMsg.data = _currentState;
		_pubState.publish(_intMsg);
        loopRate.sleep();
    }
}

private:

    double _rate = 50;

    /* Service Pursuit */
	ros::ServiceServer _servicePursuit;
    bool _pursuitActivated = false;
    bool _isPointReached = false;
    bool _prva = true;
    bool _spinning = false;
    bool _new_spinning = false;

    double _last_waypoint_x, _last_waypoint_y;

    State _currentState;

    ros::Publisher _pubGoal, _pubState;

    std_msgs::Int32 _intMsg;

    /* Odometry subscriber */
    ros::Subscriber _subOdom;

    ros::Subscriber _subPointReached;

    nav_msgs::Odometry _currOdom;

    ros::Subscriber _carrot_subscriber;

    // Todo: set as param

    // sim
    double _lat_home = -35.3632631;
    double _long_home = 149.165237;

    const double _lat_start_help = -35.3632631, _long_start_help=149.165247;

    const double _lat_start = -35.3632631, _long_start =149.165347;
    const double _lat_mid_1 = -35.3632631, _long_mid_1 =149.165447;
    const double _lat_mid_2 = -35.3632631, _long_mid_2 =149.165547;
    const double _lat_end = -35.3632631, _long_end =149.165647;
    const double _lat_end2 = -35.3632631, _long_end2 =149.165627;
    const double _global_alt = 100;




         // vani
    // const double _lat_start = 24.41772, _long_start = 54.43562;
    // const double _lat_mid_1 = 24.41767, _long_mid_1 = 54.43582;
    // const double _lat_mid_2 = 24.4176, _long_mid_2 = 54.4361;
    // const double _lat_end = 24.41756, _long_end = 54.43626;
    // const double _global_alt = 100;

    double _global_z, _yaw;

    geometry_msgs::Vector3 _currentReference;

    // std::vector<std::pair<double>> _global_waypoints{
    //         {_lat_start, _long_start},
    //         {_lat_mid_1, _long_mid_1},
    //         {_lat_mid_2, _long_mid_2},
    //         {_lat_end, _long_end}
    // };

    // For testing

    // Todo učitaj home


    // Prepare for testig
    std::vector<std::pair<double, double>> _global_waypoints{
            {_lat_home, _long_home},
            {_lat_start_help, _long_start_help},
            {_lat_start,_long_start},
            {_lat_mid_1,_long_mid_1},
            {_lat_mid_2,_long_mid_2},
            {_lat_end,  _long_end},
            {_lat_end2, _long_end2},
            {_lat_mid_2,_long_mid_2},
            {_lat_mid_1,_long_mid_1},
            {_lat_start,_long_start}
    };

    std::queue<geometry_msgs::PoseStamped> _local_waypoints;
    std::queue<geometry_msgs::PoseStamped> _spinning_waypoints;

    /* Define Dynamic Reconfigure parameters */
    boost::recursive_mutex _pursuitConfigMutex;

    Global2Local _globalToLocal;
    ros::Publisher _pubGoalsMarker;

    geometry_msgs::Point _tmp_position;
    Eigen::Vector3d _helper_position_1, _helper_position_2;
};
}

#endif /* VISUAL_SERVO_PURSUIT_STATE_MACHINE_H*/
