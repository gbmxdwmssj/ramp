#include <iostream>
#include "ros/ros.h"
#include "ramp_msgs/RampTrajectory.h"
#include "ramp_msgs/Path.h"
#include "utility.h"
#include "ramp_msgs/TrajectoryRequest.h"
#include "ramp_msgs/Population.h"
#include "ramp_msgs/BezierInfo.h"

Utility u;

int main(int argc, char** argv) {
  ros::init(argc, argv, "publish_trajectory_command");
  ros::NodeHandle handle;

  ros::Publisher pub_traj = handle.advertise<ramp_msgs::RampTrajectory>("bestTrajec", 1000);
  ros::Publisher pub_pop = handle.advertise<ramp_msgs::Population>("population", 1000);
  ros::ServiceClient client_ = handle.serviceClient<ramp_msgs::TrajectoryRequest>("trajectory_generator");





  // Build a Path
  ramp_msgs::KnotPoint c1;
  c1.motionState.positions.push_back(0.); //0.5468
  c1.motionState.positions.push_back(0.); //1.5237
  c1.motionState.positions.push_back(0.); //0.188478
  
  ramp_msgs::KnotPoint c2;
  c2.motionState.positions.push_back(0.5); // 0.5
  c2.motionState.positions.push_back(2.); // 2
  c2.motionState.positions.push_back(PI/4); //pi/4


  ramp_msgs::KnotPoint c3;
  c3.motionState.positions.push_back(2.); //0.95
  c3.motionState.positions.push_back(0.); //1.09
  c3.motionState.positions.push_back(-PI/4);  //-1.09


  ramp_msgs::KnotPoint c4;
  c4.motionState.positions.push_back(3.5);
  c4.motionState.positions.push_back(2.);
  c4.motionState.positions.push_back(PI/4);

  ramp_msgs::KnotPoint c5;
  c5.motionState.positions.push_back(4.);
  c5.motionState.positions.push_back(0.);
  c5.motionState.positions.push_back(0);
  

  // Velocities
  c1.motionState.velocities.push_back(0.);  //.115952
  c1.motionState.velocities.push_back(0.); //.0167614
  c1.motionState.velocities.push_back(-0.); //-.886569
 
  c2.motionState.velocities.push_back(0.);
  c2.motionState.velocities.push_back(0.);
  c2.motionState.velocities.push_back(0.);

  c3.motionState.velocities.push_back(0.);
  c3.motionState.velocities.push_back(0.);
  c3.motionState.velocities.push_back(0.); 
 
  c4.motionState.velocities.push_back(0);
  c4.motionState.velocities.push_back(0);
  c4.motionState.velocities.push_back(0); 
 
  c5.motionState.velocities.push_back(0);
  c5.motionState.velocities.push_back(0);
  c5.motionState.velocities.push_back(0); 


  // Accelerations
  c1.motionState.accelerations.push_back(0.); //.011033
  c1.motionState.accelerations.push_back(-0.);  //-.105006
  c1.motionState.accelerations.push_back(0.);

  c2.motionState.accelerations.push_back(0.);
  c2.motionState.accelerations.push_back(0.);
  c2.motionState.accelerations.push_back(0.);
  
  c3.motionState.accelerations.push_back(0.);
  c3.motionState.accelerations.push_back(0.);
  c3.motionState.accelerations.push_back(0.);
  
  c4.motionState.accelerations.push_back(0.);
  c4.motionState.accelerations.push_back(0.);
  c4.motionState.accelerations.push_back(0.);
  
  ramp_msgs::Path p;
  p.points.push_back(c1);
  p.points.push_back(c2);
  p.points.push_back(c3);
  //p.points.push_back(c4);
  //p.points.push_back(c5);
  

  /*ramp_msgs::BezierInfo bi;
  ramp_msgs::MotionState cp0;
  cp0.positions.push_back(0.25);
  cp0.positions.push_back(1.);
  cp0.positions.push_back(1.32582);
  cp0.velocities.push_back(0);
  cp0.velocities.push_back(0.33);
  cp0.velocities.push_back(0);
  cp0.accelerations.push_back(0);
  cp0.accelerations.push_back(0);
  cp0.accelerations.push_back(0);
  
  bi.controlPoints.push_back(cp0);
  
  ramp_msgs::MotionState sp0;
  sp0.positions.push_back(0.);
  sp0.positions.push_back(0.);
  sp0.positions.push_back(0.);
  sp0.velocities.push_back(0);
  sp0.velocities.push_back(0.);
  sp0.velocities.push_back(0);
  sp0.accelerations.push_back(0);
  sp0.accelerations.push_back(0);
  sp0.accelerations.push_back(0);
  
  bi.segmentPoints.push_back(sp0);
  bi.segmentPoints.push_back(p.points.at(1).motionState);
  bi.segmentPoints.push_back(p.points.at(2).motionState);*/
 
  // Make BezierInfo from Path
  ramp_msgs::BezierInfo bi;
  bi.segmentPoints.push_back(p.points.at(0).motionState);
  bi.segmentPoints.push_back(p.points.at(1).motionState);
  bi.segmentPoints.push_back(p.points.at(2).motionState);

  ramp_msgs::TrajectoryRequest tr;
  tr.request.path = p;
  tr.request.type = PARTIAL_BEZIER;
  //tr.request.startBezier = true;
  tr.request.print = true;
  //tr.request.bezierInfo = bi;


  std::cout<<"\nPress Enter to request and send the trajectory\n";
  std::cin.get();

  // Get and publish trajectory
  if(client_.call(tr)) {
    std::cout<<"\nSending Trajectory "<<u.toString(tr.response.trajectory);
    pub_traj.publish(tr.response.trajectory);
  }
  else {
    std::cout<<"\nSome error getting trajectory\n";
  }

  std::cout<<"\n\nPress Enter to Publish population\n";
  std::cin.get();

  // Create Population to send to trajectory_visualization
  ramp_msgs::Population pop;
  pop.population.push_back(tr.response.trajectory);
  
  pub_pop.publish(pop);


  std::cout<<"\nDifference: "<<u.findAngleFromAToB(c1.motionState.positions, c2.motionState.positions);
  std::cout<<"\nDifference: "<<u.findAngleFromAToB(c2.motionState.positions, c3.motionState.positions);


  std::cout<<"\nExiting Normally\n";
  return 0;
}
