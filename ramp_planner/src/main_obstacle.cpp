#include <ros/ros.h>
#include "obstacle.h"
#include "control_handler.h"
#include "trajectory_request_handler.h"


int main(int argc, char** argv)
{
  ros::init(argc, argv, "obstacle");
  ros::NodeHandle handle;

  ros::ServiceClient client = handle.serviceClient<ramp_msgs::TrajectoryRequest>("/trajectory_generator");
  ros::Publisher pub_traj = handle.advertise<ramp_msgs::RampTrajectory>("bestTrajec", 1000);
  ros::Publisher pub_pop = handle.advertise<ramp_msgs::Population>("/population", 1000);  
  Utility u;


  MotionState s;
  s.msg_.positions.push_back(3.5);
  s.msg_.positions.push_back(0.f);
  s.msg_.positions.push_back(1.9635);
  s.msg_.velocities.push_back(0.f);
  s.msg_.velocities.push_back(0.f);
  s.msg_.velocities.push_back(0.f);

  MotionState kp;
  kp.msg_.positions.push_back(2.7347);
  kp.msg_.positions.push_back(1.6522);
  kp.msg_.positions.push_back(1.9635);
  kp.msg_.velocities.push_back(0.f);
  kp.msg_.velocities.push_back(0.f);
  kp.msg_.velocities.push_back(0.f);

  MotionState g;
  g.msg_.positions.push_back(0.f);
  g.msg_.positions.push_back(3.5f);
  g.msg_.positions.push_back(3.f*PI/4.f);
  g.msg_.velocities.push_back(0.f);
  g.msg_.velocities.push_back(0.f);
  g.msg_.velocities.push_back(0.f);

  Path p(s,g);
  p.addBeforeGoal(kp);

  ramp_msgs::TrajectoryRequest tr;
  tr.request.path = p.buildPathMsg();
  tr.request.type = PARTIAL_BEZIER;

  ramp_msgs::BezierCurve curve;
  curve.segmentPoints.push_back(p.at(0).motionState_.msg_);
  curve.segmentPoints.push_back(p.at(1).motionState_.msg_);
  curve.segmentPoints.push_back(p.at(2).motionState_.msg_);

  tr.request.bezierCurves.push_back(curve);
  
  ROS_INFO("Press Enter to request and send the trajectory\n");
  std::cin.get();

  // Get and publish trajectory
  if(client.call(tr)) 
  {
    ROS_INFO("Got obstacle trajectory!");
  }
  else 
  {
    ROS_WARN("Some error getting obstacle trajectory");
  }

  bool cc_started = false;
  ros::Rate r(10);
  
  while(!cc_started)
  {
    ROS_INFO("In while");
    handle.getParam("/ramp/cc_started", cc_started);
    ROS_INFO("/ramp/cc_started: %s", cc_started ? "True" : "False");
    r.sleep();
    ros::spinOnce();
  }

  std::cout<<"\nSending Trajectory "<<u.toString(tr.response.trajectory)<<"\n";
  pub_traj.publish(tr.response.trajectory);
  
  // Create Population to send to trajectory_visualization
  ramp_msgs::Population pop;
  pop.population.push_back(tr.response.trajectory);
  
  pub_pop.publish(pop);

  


  return 0;
}
