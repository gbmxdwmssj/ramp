#include "move.h"



Move::Move(const ramp_msgs::Path p) : path_(p) {}


const ramp_msgs::Path Move::perform() 
{
  ROS_INFO("dir_: %f", dir_);
  ROS_INFO("dist_: %f", dist_);
  ROS_INFO("Before: %s", utility_.toString(path_).c_str()); 

  if(path_.points.size() > 2) 
  {
    double cir_theta = utility_.displaceAngle(dir_, PI);
    double cir_displace_pos = utility_.displaceAngle(cir_theta, PI/4.);
    double cir_displace_neg = utility_.displaceAngle(cir_theta, -PI/4.);

    ROS_INFO("cir_theta: %f cir_displace_pos: %f cir_displace_neg: %f", cir_theta, cir_displace_pos, cir_displace_neg);

    double robo_x = path_.points[0].motionState.positions[0];
    double robo_y = path_.points[0].motionState.positions[1];

    double ob_x = robo_x + dist_*cos(dir_);
    double ob_y = robo_y + dist_*sin(dir_);

    double ob_size = 0.45;
    ROS_INFO("robo pos: (%f, %f) ob pos: (%f, %f)", robo_x, robo_y, ob_x, ob_y);

    double pos_x = ob_x + ob_size*cos(cir_displace_pos);
    double pos_y = ob_y + ob_size*sin(cir_displace_pos);

    double neg_x = ob_x + ob_size*cos(cir_displace_neg);
    double neg_y = ob_y + ob_size*sin(cir_displace_neg);

    ROS_INFO("pos: (%f, %f) neg: (%f, %f)", pos_x, pos_y, neg_x, neg_y);

    double r_pos = sqrt( pow(pos_x - robo_x, 2) + pow(pos_y - robo_y, 2) );
    double r_neg = sqrt( pow(neg_x - robo_x, 2) + pow(neg_y - robo_y, 2) );

    double pos_theta = atan( (pos_y - robo_y) / (pos_x - robo_x) ); 
    double neg_theta = atan( (neg_y - robo_y) / (neg_x - robo_x) ); 

    double theta_diff = utility_.findDistanceBetweenAngles(pos_theta, neg_theta);
    double max_theta = (2.*PI) - theta_diff;
    
    ROS_INFO("pos_theta: %f neg_theta: %f", pos_theta, neg_theta);

    ROS_INFO("theta diff: %f max_theta: %f", theta_diff, max_theta);
    
    double theta_a = utility_.displaceAngle(dir_, PI/2.);
    double theta_b = utility_.displaceAngle(dir_, -PI/2.);
    ROS_INFO("theta_a: %f theta_b: %f", theta_a, theta_b);

    Range dir_range_pos(pos_theta, theta_a);
    Range dir_range_neg(theta_b, neg_theta);

    double theta_displacement = dir_range_pos.random();
    double theta              = utility_.displaceAngle(neg_theta, theta_displacement);

    ROS_INFO("theta_displacement: %f theta: %f", theta_displacement, theta);

    Range   dist_range(2*ob_size, 4*ob_size);
    double  dist = dist_range.random();
    ROS_INFO("dist: %f", dist);

    // Create point
    double x = cos(theta) * dist;
    double y = sin(theta) * dist;

    ROS_INFO("New point: (%f, %f)", x, y);

    // Set bounds on dimensions
    if(x < 0)
    {
      y += -x;
      x = 0;
    }
    else if(x > 3.5)
    {
      y += (x-3.5);
      x = 3.5;
    }
    if(y < 0)
    {
      x += -y;
      y = 0;
    }
    else if(y > 3.5)
    {
      x += (y-3.5);
      y = 3.5;
    }
    
    
    
    ramp_msgs::KnotPoint kp;
    kp.motionState.positions.push_back(x);
    kp.motionState.positions.push_back(y);
    kp.motionState.positions.push_back(theta);

    path_.points.at(1) = kp;
  } // end if points.size()>2

  ROS_INFO("After: %s", utility_.toString(path_).c_str()); 
  return path_;
}
