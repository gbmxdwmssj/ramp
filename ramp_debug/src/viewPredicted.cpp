#include "ros/ros.h"
#include <iostream>
#include "utility.h"
#include "ramp_msgs/Obstacle.h"
#include "ramp_msgs/Population.h"
#include "trajectory_request_handler.h"
#include "motion_type.h"
using namespace std;

ros::Publisher  pub_population;
ros::Subscriber sub_odometry;

Utility utility;
ramp_msgs::Obstacle obstacle;
TrajectoryRequestHandler* h_traj_req_;

std::vector<float> gr_angulars;
std::vector<float> tgr_linears;

bool odom_recv = false;


tf::Transform T_w_b;


/** This method determines what type of motion an obstacle has */
const MotionType findMotionType(const ramp_msgs::Obstacle ob) { 
  MotionType result;

  // Find the linear and angular velocities
  tf::Vector3 v_linear;
  tf::vector3MsgToTF(ob.odom_t.twist.twist.linear, v_linear);

  tf::Vector3 v_angular;
  tf::vector3MsgToTF(ob.odom_t.twist.twist.angular, v_angular);

  // Find magnitude of velocity vectors
  float mag_linear_t  = sqrt( tf::tfDot(v_linear, v_linear)   );
  float mag_angular_t = sqrt( tf::tfDot(v_angular, v_angular) );


  // Translation only
  // normally 0.0066 when idle
  if(mag_linear_t >= 0.15 && mag_angular_t < 0.1) {
    result = MotionType::Translation;
  }

  // Self-Rotation
  // normally 0.053 when idle
  else if(mag_linear_t < 0.15 && mag_angular_t >= 0.1) {
    result = MotionType::Rotation;
  }

  // Either translation+self-rotation or global rotation
  else if(mag_linear_t >= 0.15 && mag_angular_t >= 0.1) {
    result = MotionType::TranslationAndRotation;
  } //end else if

  // Else, there is no motion
  else {
    result = MotionType::None;
  }

  return result;
} // End findMotionType




const ramp_msgs::Path getObstaclePath(const ramp_msgs::Obstacle ob, const MotionType mt) {
  ramp_msgs::Path result;

  std::vector<ramp_msgs::KnotPoint> path;

  ros::Duration predictionTime_(5);

  // Create and initialize the first point in the path
  ramp_msgs::KnotPoint start;
  start.motionState.positions.push_back(ob.odom_t.pose.pose.position.x);
  start.motionState.positions.push_back(ob.odom_t.pose.pose.position.y);
  start.motionState.positions.push_back(tf::getYaw(ob.odom_t.pose.pose.orientation));

  // Push the first point onto the path
  path.push_back(start);

  /** Find the ending configuration for the predicted trajectory based on motion type */
  // If translation
  if(mt == MotionType::Translation) {

    // Create the Goal Knotpoint
    ramp_msgs::KnotPoint goal;

    // Get the goal position in the base frame
    tf::Vector3 ob_goal_b(start.motionState.positions.at(0) + (ob.odom_t.twist.twist.linear.x * predictionTime_.toSec()), 
                          start.motionState.positions.at(1) + (ob.odom_t.twist.twist.linear.y * predictionTime_.toSec()),
                          0);

    // Convert the goal position to world coordinates
    tf::Vector3 goal_w = T_w_b * ob_goal_b;
    
    // Push on the world coordinates
    goal.motionState.positions.push_back(goal_w.getX());
    goal.motionState.positions.push_back(goal_w.getY());
    goal.motionState.positions.push_back(start.motionState.positions.at(2));
    
    // Push goal onto the path
    path.push_back(goal);
  } // end if translation

  // If translation and rotation
  else if(mt == MotionType::TranslationAndRotation) {

    // Find the linear and angular velocity vectors
    tf::Vector3 v_linear;
    tf::Vector3 v_angular;
    tf::vector3MsgToTF(ob.odom_t.twist.twist.linear, v_linear);
    tf::vector3MsgToTF(ob.odom_t.twist.twist.angular, v_angular);

    // Find magnitudes of velocity vectors and radius r
    float v = sqrt( tf::tfDot(v_linear, v_linear)   );
    float w = sqrt( tf::tfDot(v_angular, v_angular) );
    float r = v / w;
    //std::cout<<"\nv: "<<v<<" w: "<<w<<" r: "<<r;

    // Find the angle from base origin to robot position for polar coordinates
    tf::Vector3 a(0, 0, 0);
    tf::Vector3 b(ob.odom_t.pose.pose.position.x, ob.odom_t.pose.pose.position.y, 0);
    float polar_theta_r = utility.findAngleFromAToB(a, b);

    // Find the radius from base origin to robot position for polar coordinates
    float polar_r_r = sqrt(pow(start.motionState.positions.at(0),2) + pow(start.motionState.positions.at(1), 2));
    
    //std::cout<<"\npolar_theta_r: "<<polar_theta_r;
    //std::cout<<"\npolar_r_r: "<<polar_r_r;

    // Generate intermediate points for circlular motion
    for(float i=0.25f;i<predictionTime_.toSec();i+=0.25f) {

      // Create new knot point for the path
      ramp_msgs::KnotPoint temp;

      // Get the polar coordinates theta value in base frame 
      float theta_prime_r = utility.displaceAngle(polar_theta_r, w*i);

      // Convert from polar to cartesian in base frame
      float x_prime_r = polar_r_r * cos(theta_prime_r);
      float y_prime_r = polar_r_r * sin(theta_prime_r);
      float theta_r = utility.displaceAngle(start.motionState.positions.at(2), w*i);
      //std::cout<<"\nx_prime_r: "<<x_prime_r<<" y_prime_r: "<<y_prime_r<<" theta_r: "<<theta_r;

      // Now convert position in base frame to world coordinates
      tf::Vector3 p_r(x_prime_r, y_prime_r, 0);
      tf::Vector3 p_w = T_w_b * p_r;

      // Push the values onto temp
      temp.motionState.positions.push_back(p_w.getX());
      temp.motionState.positions.push_back(p_w.getY());
      temp.motionState.positions.push_back(utility.displaceAngle(theta_r, tf::getYaw(T_w_b.getRotation())));
      
      // Push temp onto path
      path.push_back(temp);
    } // end for
  } // end else if
  



  // If rotation
  // Since our robot models are circles, rotation is the same as no movement
  else if(mt == MotionType::Rotation || mt == MotionType::None) {
    
    // Create the Goal Knotpoint
    ramp_msgs::KnotPoint goal;
    tf::Vector3 ob_goal(start.motionState.positions.at(0), start.motionState.positions.at(1), 0);
    tf::Vector3 goal_w = T_w_b * ob_goal;

    
    // Push on the world coordinates
    goal.motionState.positions.push_back(goal_w.getX());
    goal.motionState.positions.push_back(goal_w.getY());
    goal.motionState.positions.push_back(start.motionState.positions.at(2));

    path.push_back(goal);
  } // end if self-rotation, none


  // Convert the starting point to world coordinates
  tf::Vector3 start_w(start.motionState.positions.at(0), start.motionState.positions.at(1), 0);
  start_w = T_w_b * start_w;
  path.at(0).motionState.positions.at(0) = start_w.getX();
  path.at(0).motionState.positions.at(1) = start_w.getY();
  path.at(0).motionState.positions.at(2) = utility.displaceAngle(start.motionState.positions.at(2), tf::getYaw(T_w_b.getRotation()));


  result = utility.getPath(path);
  return result; 
}






/** This method returns the predicted trajectory for an obstacle for the future duration d 
 * TODO: Remove Duration parameter and make the predicted trajectory be computed until robot reaches bounds of environment */
const ramp_msgs::RampTrajectory getPredictedTrajectory(const ramp_msgs::Obstacle ob, const ros::Duration d) {
  ramp_msgs::RampTrajectory result;

  // First, identify which type of trajectory it is
  // translations only, self-rotation, translation and self-rotation, or global rotation
  MotionType motion_type = findMotionType(ob);
  

  // Now build a Trajectory Request 
  ramp_msgs::TrajectoryRequest tr;
    tr.request.path = getObstaclePath(ob, motion_type);
    tr.request.resolutionRate = 5;

  // Get trajectory
  if(h_traj_req_->request(tr)) {
    result = tr.response.trajectory;
  }

  return result;
} //End getPredictedTrajectory





void odometryCallback(const nav_msgs::Odometry& msg) {
  obstacle.odom_t_prev = obstacle.odom_t;
  obstacle.odom_t = msg;

  odom_recv = true;
  
  MotionType motion_type = findMotionType(obstacle);
  if(motion_type == MotionType::Translation) {
    std::cout<<"\nmotion_type == Translation";
  }
  else if(motion_type == MotionType::Rotation) {
    std::cout<<"\nmotion_type == Rotation";
  }
  else if(motion_type == MotionType::TranslationAndRotation) {
    std::cout<<"\nmotion_type == Translation + Rotation";
  }
  else if(motion_type == MotionType::None) {
    std::cout<<"\nmotion_type == None";
  }
}




void getAndSendTrajectory() {

  if(odom_recv) {
    ros::Duration d(10);
    ramp_msgs::RampTrajectory t = getPredictedTrajectory(obstacle, d);

    ramp_msgs::Population pop;
    pop.population.push_back(t);
    pop.best_id  = 0;
    pop.robot_id = 1;

    pub_population.publish(pop);
  }
}


void fakeOdom() {
  obstacle.odom_t.pose.pose.position.x = 0.5;
  obstacle.odom_t.pose.pose.position.y = 0.86602;
  tf::quaternionTFToMsg(tf::createQuaternionFromYaw(2.356f), obstacle.odom_t.pose.pose.orientation);
  obstacle.odom_t.twist.twist.linear.x = 0.25;
  obstacle.odom_t.twist.twist.linear.y = 0.25;
  obstacle.odom_t.twist.twist.angular.z = 0.25;
  odom_recv = true;
}



int main(int argc, char** argv) {
  ros::init(argc, argv, "viewPredicted");
  ros::NodeHandle handle;

  h_traj_req_ = new TrajectoryRequestHandler(handle);

  pub_population = handle.advertise<ramp_msgs::Population>("population", 1000);
  sub_odometry = handle.subscribe("odometry", 1000, odometryCallback);
  

  T_w_b.setOrigin(tf::Vector3(0, 0, 0));
  T_w_b.setRotation(tf::createQuaternionFromYaw(0));

  ros::Rate r(1);
  std::cout<<"\nWaiting for obstacle's odometry to be published..\n";
//  fakeOdom();
  while(ros::ok()) {
    ros::spinOnce();
    getAndSendTrajectory();
    r.sleep();
  }



  /* Show the max and average velocities */
  float max_w = gr_angulars.at(0);
  float w_sum = max_w;
  for(unsigned int i=1;i<gr_angulars.size();i++) {
    if(gr_angulars.at(i) > max_w) {
      max_w = gr_angulars.at(i);
    }
    w_sum += gr_angulars.at(i);
  }

  float max_v = tgr_linears.at(0);
  float v_sum = max_v;
  for(unsigned int i=1;i<tgr_linears.size();i++) {
    if(tgr_linears.at(i) > max_v) {
      max_v = tgr_linears.at(i);
    }
    v_sum += tgr_linears.at(i);
  }

  float avg_w = w_sum / gr_angulars.size();
  float avg_v = v_sum / tgr_linears.size();

  std::cout<<"\nmax angular velocity when driving straight: "<<max_w;
  std::cout<<"\naverage angular velocity when driving straight: "<<avg_w;
  std::cout<<"\nmax linear velocity when self-rotating: "<<max_v;
  std::cout<<"\naverage linear velocity when self-rotating: "<<avg_v;
  cout<<"\nExiting Normally\n";
  return 0;
}
