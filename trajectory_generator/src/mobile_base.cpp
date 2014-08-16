#include "mobile_base.h"





/** Constructor */
MobileBase::MobileBase() {
  
  // Set DOF
  reflexxesData_.NUMBER_OF_DOFS = 3;

  // Creating all relevant objects of the Type II Reflexxes Motion Library
  reflexxesData_.rml = new ReflexxesAPI( reflexxesData_.NUMBER_OF_DOFS, CYCLE_TIME_IN_SECONDS );
  reflexxesData_.inputParameters = new RMLPositionInputParameters( reflexxesData_.NUMBER_OF_DOFS );
  reflexxesData_.outputParameters = new RMLPositionOutputParameters( reflexxesData_.NUMBER_OF_DOFS );
  
  // Use time synchronization so the robot drives in a straight line towards goal 
  reflexxesData_.flags.SynchronizationBehavior = RMLPositionFlags::ONLY_TIME_SYNCHRONIZATION;


  // Set up the motion constraints (max velocity, acceleration and jerk)
  // Maximum velocity   
  reflexxesData_.inputParameters->MaxVelocityVector->VecData[0] = 0.33;
  reflexxesData_.inputParameters->MaxVelocityVector->VecData[1] = 0.33;
  reflexxesData_.inputParameters->MaxVelocityVector->VecData[2] = PI/4;
  

  // Maximum acceleration
  reflexxesData_.inputParameters->MaxAccelerationVector->VecData[0] = 0.66;
  reflexxesData_.inputParameters->MaxAccelerationVector->VecData[1] = 0.66;
  reflexxesData_.inputParameters->MaxAccelerationVector->VecData[2] = PI/4;
  

  // As the maximum jerk values are not known, this is just to try
  reflexxesData_.inputParameters->MaxJerkVector->VecData[0] = 1;
  reflexxesData_.inputParameters->MaxJerkVector->VecData[1] = 1;
  reflexxesData_.inputParameters->MaxJerkVector->VecData[2] = PI/3;

  
  
  // Result
  resultValue = 0;


  // Set the time to cutoff generating points
  timeCutoff_ = ros::Duration(3.5);

  // Starting time
  t_started_ = ros::Time::now();
} 




/** Destructor */
MobileBase::~MobileBase() {
  delete reflexxesData_.rml;
  delete reflexxesData_.inputParameters;
  delete reflexxesData_.outputParameters;
}





/** This method sets the new target of Reflexxes */
void MobileBase::setTarget(const ramp_msgs::MotionState ms) {
  
  // For each DOF, set the targets for the knot point
  for(unsigned int i=0;i<reflexxesData_.NUMBER_OF_DOFS;i++) {
    reflexxesData_.inputParameters->TargetPositionVector->VecData[i] = ms.positions.at(i);
    if(ms.velocities.size() > 0) {
      reflexxesData_.inputParameters->TargetVelocityVector->VecData[i] = ms.velocities.at(i);
    }
  }  
} // End setTarget







/** This method sets the SelectionVector based on the path p */
void MobileBase::setSelectionVector() {

  reflexxesData_.inputParameters->SelectionVector->VecData[0] = true;
  reflexxesData_.inputParameters->SelectionVector->VecData[1] = true;
  reflexxesData_.inputParameters->SelectionVector->VecData[2] = false;
}




void MobileBase::setSelectionVectorRotation() {
  reflexxesData_.inputParameters->SelectionVector->VecData[0] = false;
  reflexxesData_.inputParameters->SelectionVector->VecData[1] = false;
  reflexxesData_.inputParameters->SelectionVector->VecData[2] = true;
}


/**
 * Initialize variables just after receiving a service request
 * Set-up the input parameters
 * The first degree of freedom is x position
 * The second degree of freedom is y position
 **/
void MobileBase::setInitialConditions() {
  
  // Initialise the time
  timeFromStart_ = ros::Duration(0);
  

  // Set the position and orientation of the robot as Reflexxes input
  for(unsigned int i=0;i<reflexxesData_.NUMBER_OF_DOFS;i++) {
    reflexxesData_.inputParameters->CurrentPositionVector->VecData[i] = path_.points[0].motionState.positions.at(i);
  }

  // Set the current velocities, in the reference frame, as Reflexxes input
  // This is the latest velocity value got from the path
  if(path_.points.at(0).motionState.velocities.size() > 0) {
    for(unsigned int i=0;i<reflexxesData_.NUMBER_OF_DOFS;i++) {
      reflexxesData_.inputParameters->CurrentVelocityVector->VecData[i] = path_.points.at(0).motionState.velocities.at(i);
    }
  }
  else {//log some error
  }

  // Set the current accelerations, in the reference frame, as Reflexxes input
  // This is the latest acceleration value got from the path
  if(path_.points.at(0).motionState.accelerations.size() > 0) {
    for(unsigned int i=0;i<reflexxesData_.NUMBER_OF_DOFS;i++) {
      reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[i] = path_.points.at(0).motionState.accelerations.at(i);
    }
  }
  else {//log some error
  }
} // End setInitialConditions





// Compute the orientation needed to reach the target, given an initial position
double MobileBase::computeTargetOrientation(double initial_x, double initial_y, double target_x, double target_y) {
  //We need to recalculate the target orientation
  // For that we first need to create a vector for the current position and the target position
  std::vector<double> current_position;
  current_position.push_back(initial_x);
  current_position.push_back(initial_y);

  std::vector<double> target_position;
  target_position.push_back(target_x);
  target_position.push_back(target_y);

  double angle = utility_.findAngleFromAToB(current_position, target_position);

  return angle;
}





void MobileBase::init(const ramp_msgs::TrajectoryRequest::Request req) {

  // Store the path
  path_ = req.path;

  print_ = req.print;
  

  switch(req.type) {
    case ALL_STRAIGHT_SEGMENTS:
      bezier_ = false;
      partial_ = false;
      transition_ = false;
      break;
    case ALL_BEZIER:
      bezier_ = true;
      partial_ = false;
      transition_ = false;
      break;
    case PARTIAL_BEZIER:
      bezier_ = false;
      partial_ = true;
      transition_ = false;
      break;
    case TRANSITION:
      bezier_ = false;
      partial_ = false;
      transition_ = true;
  }
 
  // Set the initial conditions of the reflexxes library
  setInitialConditions();

  // Set SelectionVector
  setSelectionVector();

}

void MobileBase::insertPoint(const ramp_msgs::MotionState ms, ramp_msgs::TrajectoryRequest::Response& res) {
  trajectory_msgs::JointTrajectoryPoint jp = utility_.getTrajectoryPoint(ms);
  jp.time_from_start = timeFromStart_;
  insertPoint(jp, res);
}


void MobileBase::insertPoint(const trajectory_msgs::JointTrajectoryPoint jp, ramp_msgs::TrajectoryRequest::Response& res) {
  res.trajectory.trajectory.points.push_back(jp);

  /** Update Reflexxes */
  timeFromStart_ += ros::Duration(CYCLE_TIME_IN_SECONDS);
  
  reflexxesData_.inputParameters->CurrentPositionVector->VecData[0] = jp.positions.at(0);
  reflexxesData_.inputParameters->CurrentPositionVector->VecData[1] = jp.positions.at(1);
  reflexxesData_.inputParameters->CurrentPositionVector->VecData[2] = jp.positions.at(2);
  
  
  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[0] = jp.velocities.at(0);
  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[1] = jp.velocities.at(1);
  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[2] = jp.velocities.at(2);
  
  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[0] = jp.accelerations.at(0);
  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[1] = jp.accelerations.at(1);
  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[2] = jp.accelerations.at(2);

}



/** TODO: get x_dot_0, y_dot_0 when they're not 0 */
const double MobileBase::findVelocity(const uint8_t i, const double s) const {
  
  double a = (2.*reflexxesData_.inputParameters->MaxAccelerationVector->VecData[i]/3.);

  double x_dot_init = reflexxesData_.inputParameters->CurrentPositionVector->VecData[0];
  double y_dot_init = reflexxesData_.inputParameters->CurrentPositionVector->VecData[1];

  double radicand = (2*a*s) + pow(x_dot_init, 2);
  double v = sqrt(radicand);
  
  double t = (v - x_dot_init) / a;
  
  /*std::cout<<"\ni: "<<(int)i;
  std::cout<<"\ns: "<<s<<" a: "<<a<<" x_dot_init: "<<x_dot_init<<" y_dot_init: "<<y_dot_init;
  std::cout<<"\nradicand: "<<radicand;
  std::cout<<"\nv: "<<v;
  std::cout<<"\nt: "<<t;*/
    

  // Check for bounds
  if(v > reflexxesData_.inputParameters->MaxVelocityVector->VecData[i]) {
    v = reflexxesData_.inputParameters->MaxVelocityVector->VecData[i];
  }
  if(v < -reflexxesData_.inputParameters->MaxVelocityVector->VecData[i]) {
    v = -reflexxesData_.inputParameters->MaxVelocityVector->VecData[i];
  }


  return v;
}



const bool MobileBase::lambdaOkay(const std::vector<ramp_msgs::MotionState> segment_points, const double lambda) const {
  ramp_msgs::MotionState X0, X1, X2, p0, p1, p2;

  p0 = segment_points.at(0);
  p1 = segment_points.at(1);
  p2 = segment_points.at(2);
  
  X1 = segment_points.at(1);
  
  X0.positions.push_back( (1-lambda)*p0.positions.at(0) + lambda*p1.positions.at(0) );
  X0.positions.push_back( (1-lambda)*p0.positions.at(1) + lambda*p1.positions.at(1) );
  X0.positions.push_back(utility_.findAngleFromAToB(p0.positions, p1.positions));

  X2.positions.push_back( (1-lambda)*p1.positions.at(0) + lambda*p2.positions.at(0) );
  X2.positions.push_back( (1-lambda)*p1.positions.at(1) + lambda*p2.positions.at(1) );
  X2.positions.push_back(utility_.findAngleFromAToB(p1.positions, p2.positions));


  if(X1.positions.at(0) == ( (X0.positions.at(0) + X2.positions.at(0)) / 2. ) &&
      X1.positions.at(1) == ( (X0.positions.at(1) + X2.positions.at(1)) / 2. )) {
    return false;
  }

  return true;
}





const double MobileBase::getControlPointLambda(const std::vector<ramp_msgs::MotionState> segment_points) const {
  std::vector<double> result;

  double lambda = transition_ ? 0.5 : 0.5;

  while(!lambdaOkay(segment_points, lambda)) {
    if(transition_) {
      lambda-=0.1; 
    }
    else {
      lambda+=0.1;
    }

    // TODO: Report/Handle error
    if(lambda < 0.1) {
      lambda = 0.9;
    }

    if(lambda > 0.9) {
      lambda = 0.1;
    }
  }

  return lambda;
} // End getControlPointLambda





const std::vector<BezierCurve> MobileBase::bezier(ramp_msgs::Path& p, const bool only_curve) {
  std::vector<BezierCurve> result;
  double lambda = 0.5;

  ramp_msgs::Path p_copy = p;

  int stop = partial_ ? stop = 2 : p_copy.points.size()-1;

  //std::cout<<"\np.points.size(): "<<p.points.size()<<"\n";
  for(uint8_t i=1;i<stop;i++) {
    //std::cout<<"\n---i: "<<(int)i<<"---\n";
    BezierCurve bc;
    bc.print_ = print_;

    std::vector<ramp_msgs::MotionState> segment_points;
    segment_points.push_back(p_copy.points.at(i-1).motionState);
    segment_points.push_back(p_copy.points.at(i).motionState);
    segment_points.push_back(p_copy.points.at(i+1).motionState);

    /*std::cout<<"\nSegment points: ";
    for(uint8_t a=0;a<segment_points.size();a++) {
      std::cout<<"\n "<<a<<"\n"<<utility_.toString(segment_points.at(a));
    }*/


    // Find the slope
    double ryse = segment_points.at(1).positions.at(1) - segment_points.at(0).positions.at(1);
    double run  = segment_points.at(1).positions.at(0) - segment_points.at(0).positions.at(0);
    double ryse2 = segment_points.at(2).positions.at(1) - segment_points.at(1).positions.at(1);
    double run2  = segment_points.at(2).positions.at(0) - segment_points.at(1).positions.at(0);

    double slope  = (run != 0) ? ryse / run : ryse;
    double slope2 = (run2 != 0) ? ryse2 / run2 : ryse2;
    double x_dot_0, y_dot_0, x_dot_max, y_dot_max, x_dot_02, y_dot_02, x_dot_max2, y_dot_max2;
    uint8_t i_max;
    
    //std::cout<<"\nslope: "<<slope;
    //std::cout<<"\nryse: "<<ryse<<" run: "<<run;

    // Segment 1 size
    double s = lambda * utility_.positionDistance(segment_points.at(0).positions, segment_points.at(1).positions);
    //std::cout<<"\nAfter s\n";


    /*** Segment 1 ***/
    // If the y is greater
    if(slope >= 1) {
      y_dot_0 = findVelocity(1, s);
      x_dot_0 = y_dot_0 / slope;  
      i_max = 1;

      y_dot_max = y_dot_0;
      x_dot_max = y_dot_max / slope;

    }
    else if(slope == -1 && ryse < 0) {
      x_dot_0 = findVelocity(0, s);
      y_dot_0 = x_dot_0 * slope;
      i_max = 1;

      x_dot_max = x_dot_0;
      y_dot_max = x_dot_max * slope;
    }
    else if(slope == -1 && run < 0) {
      y_dot_0 = findVelocity(1, s);
      x_dot_0 = y_dot_0 / slope;
      i_max = 1;

      y_dot_max = y_dot_0;
      x_dot_max = y_dot_max / slope;
    }
    else if(slope < -1) {
      y_dot_0 = findVelocity(1, s);
      x_dot_0 = y_dot_0 / slope;
      i_max = 1;

      y_dot_max = y_dot_0;
      x_dot_max = y_dot_max / slope;
    } 
    else if(slope < 0) {
      x_dot_0 = findVelocity(0, s);
      y_dot_0 = x_dot_0 * slope;
      i_max = 1;

      x_dot_max = x_dot_0;
      y_dot_max = x_dot_max * slope;
    }
    else {
      x_dot_0 = findVelocity(0, s);
      y_dot_0 = x_dot_0 * slope;  
      i_max = 0;
      
      x_dot_max = x_dot_0;
      y_dot_max = slope*x_dot_max;
    }
    //std::cout<<"\nx_dot_0: "<<x_dot_0<<" y_dot_0: "<<y_dot_0;
    

    /*** Segment 2 ***/
    // If the y is greater
    if(slope2 >= 1) {
      y_dot_02 = findVelocity(1, s);
      x_dot_02 = y_dot_02 / slope2;  
      i_max = 1;

      y_dot_max2 = y_dot_02;
      x_dot_max2 = y_dot_max2 / slope2;

    }
    else if(slope2 == -1 && ryse < 0) {
      x_dot_02 = findVelocity(0, s);
      y_dot_02 = x_dot_02 * slope2;
      i_max = 1;

      x_dot_max2 = x_dot_02;
      y_dot_max2 = x_dot_max2 * slope2;
    }
    else if(slope2 == -1 && run < 0) {
      y_dot_02 = findVelocity(1, s);
      x_dot_02 = y_dot_02 / slope2;
      i_max = 1;

      y_dot_max2 = y_dot_02;
      x_dot_max2 = y_dot_max2 / slope2;
    }
    else if(slope2 < -1) {
      y_dot_02 = findVelocity(1, s);
      x_dot_02 = y_dot_02 / slope2;
      i_max = 1;

      y_dot_max2 = y_dot_02;
      x_dot_max2 = y_dot_max2 / slope2;
    } 
    else if(slope2 < 0) {
      x_dot_02 = findVelocity(0, s);
      y_dot_02 = x_dot_02 * slope2;
      i_max = 1;

      x_dot_max2 = x_dot_02;
      y_dot_max2 = x_dot_max2 * slope2;
    }
    else {
      x_dot_02 = findVelocity(0, s);
      y_dot_02 = x_dot_02 * slope2;  
      i_max = 0;
      
      x_dot_max2 = x_dot_02;
      y_dot_max2 = slope*x_dot_max2;
    }
    //std::cout<<"\nx_dot_0: "<<x_dot_0<<" y_dot_0: "<<y_dot_0;

    double theta = utility_.findAngleFromAToB(segment_points.at(0).positions, segment_points.at(1).positions);



    double lambda = getControlPointLambda(segment_points);
    //std::cout<<"\nlambda: "<<lambda;
   

    if(transition_) {
      bc.init(segment_points, 0, theta, 
          segment_points.at(0).velocities.at(0),
          segment_points.at(0).velocities.at(1),
          x_dot_02, y_dot_02,
          segment_points.at(0).accelerations.at(0),
          segment_points.at(0).accelerations.at(1),
          x_dot_max, y_dot_max,
          x_dot_max2, y_dot_max2,
          reflexxesData_.inputParameters->MaxAccelerationVector->VecData[0],
          reflexxesData_.inputParameters->MaxAccelerationVector->VecData[1]);      

    } 
    else {
       bc.init(segment_points, lambda, theta, 
         x_dot_0, y_dot_0, 
         x_dot_02, y_dot_02,         
         0, 0,
         x_dot_max, y_dot_max, 
         x_dot_max2, y_dot_max2,
         reflexxesData_.inputParameters->MaxAccelerationVector->VecData[0],
         reflexxesData_.inputParameters->MaxAccelerationVector->VecData[1]);
    }


    std::cout<<"\nBefore generateCurve\n";
    bc.generateCurve();
    std::cout<<"\nAfter generateCurve\n";

    if(bc.points_.size() > 0) {
      
      // Insert the first point as a knot point in the path
      // The first point is control point c0 with v and a information
      // If it's a transition, the first point on curve is the first point that's already there
      if(!transition_) {
        p.points.erase(p.points.begin()+i);
        p.points.insert(p.points.begin()+i, utility_.getKnotPoint(bc.points_.at(0)));
        p.points.insert(p.points.begin()+i+1, utility_.getKnotPoint(bc.points_.at(bc.points_.size()-1)));
      }
      
      // Else, erase the 2nd control point
      else if (bc.points_.size() > 0) {
        std::cout<<"\nErasing point "<<utility_.toString(p.points.at(1))<<"\n";
        p.points.erase(p.points.begin()+1);
        p.points.insert(p.points.begin()+i, utility_.getKnotPoint(bc.points_.at(bc.points_.size()-1)));
      }

      // Insert the last point as a knot point in the path
      // The last point is control point c2 with v and a information
      std::cout<<"\nInserting point "<<utility_.toString(bc.points_.at(bc.points_.size()-1))<<"\n";

      result.push_back(bc);
    } // end if
  } // end for
  


  //std::cout<<"\nPath after Bezier: "<<utility_.toString(result)<<"\n";
  return result;
} // End bezier





/** Execute one iteration of the Reflexxes control function */
const trajectory_msgs::JointTrajectoryPoint MobileBase::spinOnce() {

  // Calling the Reflexxes OTG algorithm
  resultValue = reflexxesData_.rml->RMLPosition(*reflexxesData_.inputParameters, reflexxesData_.outputParameters, reflexxesData_.flags);


  /** Build the JointTrajectoryPoint object that will be used to build the trajectory */
  trajectory_msgs::JointTrajectoryPoint point = buildTrajectoryPoint(*reflexxesData_.inputParameters, *reflexxesData_.outputParameters);
  
  /*std::cout<<"\n\nCalled reflexxes with input:";
  std::cout<<"\nreflexxesData_.inputParameters->CurrentPositionVector->VecData[0]: "<<
                reflexxesData_.inputParameters->CurrentPositionVector->VecData[0];
  std::cout<<"\nreflexxesData_.inputParameters->CurrentPositionVector->VecData[1]: "<<
                reflexxesData_.inputParameters->CurrentPositionVector->VecData[1];
  std::cout<<"\nreflexxesData_.inputParameters->CurrentPositionVector->VecData[2]: "<<
                reflexxesData_.inputParameters->CurrentPositionVector->VecData[2];
  
  std::cout<<"\n\nreflexxesData_.inputParameters->CurrentVelocityVector->VecData[0]: "<<
                  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[0];
  std::cout<<"\nreflexxesData_.inputParameters->CurrentVelocityVector->VecData[1]: "<<
                reflexxesData_.inputParameters->CurrentVelocityVector->VecData[1];
  std::cout<<"\nreflexxesData_.inputParameters->CurrentVelocityVector->VecData[2]: "<<
                reflexxesData_.inputParameters->CurrentVelocityVector->VecData[2];
  
  std::cout<<"\n\nreflexxesData_.inputParameters->CurrentAccelerationVector->VecData[0]: "<<
                  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[0];
  std::cout<<"\nreflexxesData_.inputParameters->CurrentAccelerationVector->VecData[1]: "<<
                reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[1];
  std::cout<<"\nreflexxesData_.inputParameters->CurrentAccelerationVector->VecData[2]: "<<
                reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[2];
  
  
  std::cout<<"\n\nOutput: ";
  std::cout<<"\nreflexxesData_.outputParameters->NewPositionVector->VecData[0]: "<<
                reflexxesData_.outputParameters->NewPositionVector->VecData[0];
  std::cout<<"\nreflexxesData_.outputParameters->NewPositionVector->VecData[1]: "<<
                reflexxesData_.outputParameters->NewPositionVector->VecData[1];
  
  std::cout<<"\nreflexxesData_.outputParameters->NewVelocityVector->VecData[0]: "<<
                reflexxesData_.outputParameters->NewVelocityVector->VecData[0];
  std::cout<<"\nreflexxesData_.outputParameters->NewVelocityVector->VecData[1]: "<<
                reflexxesData_.outputParameters->NewVelocityVector->VecData[1];
  
  std::cout<<"\nreflexxesData_.outputParameters->NewAccelerationVector->VecData[0]: "<<
                reflexxesData_.outputParameters->NewAccelerationVector->VecData[0];
  std::cout<<"\nreflexxesData_.outputParameters->NewAccelerationVector->VecData[1]: "<<
                reflexxesData_.outputParameters->NewAccelerationVector->VecData[1];*/



  // The input of the next iteration is the output of this one
  *reflexxesData_.inputParameters->CurrentPositionVector = *reflexxesData_.outputParameters->NewPositionVector;
  *reflexxesData_.inputParameters->CurrentVelocityVector = *reflexxesData_.outputParameters->NewVelocityVector;
  *reflexxesData_.inputParameters->CurrentAccelerationVector = *reflexxesData_.outputParameters->NewAccelerationVector;

  return point;
} // End spinOnce







/** This method will return a JointTrajectoryPoint given some output parameters from Reflexxes */
const trajectory_msgs::JointTrajectoryPoint MobileBase::buildTrajectoryPoint(const RMLPositionInputParameters input, const RMLPositionOutputParameters output) {
  trajectory_msgs::JointTrajectoryPoint point;

  
  // Push on the p, v, and a vectors
  for(unsigned int i=0;i<reflexxesData_.NUMBER_OF_DOFS;i++) {
    if(reflexxesData_.inputParameters->SelectionVector->VecData[i]) {
      point.positions.push_back(output.NewPositionVector->VecData[i]);
      point.velocities.push_back(output.NewVelocityVector->VecData[i]);
      point.accelerations.push_back(output.NewAccelerationVector->VecData[i]);
    }

    else if(i == 2) {
      
      // If straight-line paths, make theta be towards next knot point
      double theta = computeTargetOrientation(prevKP_.positions.at(0),
                                              prevKP_.positions.at(1),
                                              input.TargetPositionVector->VecData[0],
                                              input.TargetPositionVector->VecData[1]);

      // Push on theta
      point.positions.push_back(theta);

      // Get angular velocity
      double w = (theta - input.CurrentPositionVector->VecData[2]) / CYCLE_TIME_IN_SECONDS;
      point.velocities.push_back(w);
      point.accelerations.push_back(0);


      reflexxesData_.outputParameters->NewPositionVector->VecData[2] = theta;
      reflexxesData_.outputParameters->NewVelocityVector->VecData[2] = w;
     
    }

    else {
      point.positions.push_back(reflexxesData_.inputParameters->CurrentPositionVector->VecData[i]);
      point.velocities.push_back(reflexxesData_.inputParameters->CurrentVelocityVector->VecData[i]);
      point.accelerations.push_back(reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[i]);
    }
  }


  // The timeFromStart_ is the time of the previous point plus the cycle period
  point.time_from_start = timeFromStart_;
  timeFromStart_ += ros::Duration(CYCLE_TIME_IN_SECONDS);


  return point;
} // End buildTrajectoryPoint








/** 
 *  This method will return a JointTrajectoryPoint given some input parameters from Reflexxes 
 *  This is used to generate the first point on a trajectory
 **/
const trajectory_msgs::JointTrajectoryPoint MobileBase::buildTrajectoryPoint(const RMLPositionInputParameters my_inputParameters) {
  trajectory_msgs::JointTrajectoryPoint point;

  
  // Push on the p, v, and a vectors
  for(unsigned int i=0;i<reflexxesData_.NUMBER_OF_DOFS;i++) {
    if(reflexxesData_.inputParameters->SelectionVector->VecData[i]) {
      point.positions.push_back(my_inputParameters.CurrentPositionVector->VecData[i]);
      point.velocities.push_back(my_inputParameters.CurrentVelocityVector->VecData[i]);
      point.accelerations.push_back(my_inputParameters.CurrentAccelerationVector->VecData[i]);
    }

    else {
      point.positions.push_back(reflexxesData_.inputParameters->CurrentPositionVector->VecData[i]);
      point.velocities.push_back(reflexxesData_.inputParameters->CurrentVelocityVector->VecData[i]);
      point.accelerations.push_back(reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[i]);
    }
  }


  // The timeFromStart_ is the time of the previous point plus the cycle period
  point.time_from_start = timeFromStart_;
  timeFromStart_ += ros::Duration(CYCLE_TIME_IN_SECONDS);


  return point;
} // End buildTrajectoryPoint






// Service callback, the input is a path and the output a trajectory
bool MobileBase::trajectoryRequest(ramp_msgs::TrajectoryRequest::Request& req, ramp_msgs::TrajectoryRequest::Response& res) {
  //std::cout<<"\nTrajectory Request Received: "<<utility_.toString(req)<<"\n";

  // If there's less than 3 points,
  // make it have straight segments
  if(req.path.points.size() < 3 && req.type != TRANSITION) {
    req.type = ALL_STRAIGHT_SEGMENTS;
  }
  
  // Initialize Reflexxes with request
  init(req);

  // Set start time
  t_started_ = ros::Time::now();

  // Set the trajectory's resolution rate
  res.trajectory.resolution_rate = CYCLE_TIME_IN_SECONDS;

  std::vector<BezierCurve> curves;

  // Use Bezier curves to smooth path
  if(bezier_ || partial_ || transition_) {
    if(print_) 
      std::cout<<"\nPath before Bezier: "<<utility_.toString(path_)<<"\n";
    curves = bezier(path_, transition_);
    if(print_)
      std::cout<<"\n*******************Path after Bezier: "<<utility_.toString(path_)<<"\n";
  }

  if(print_) {
    for(int c=0;c<curves.size();c++) {
      std::cout<<"\nCurve "<<c<<": ";
      for(int p=0;p<curves.at(c).points_.size();p++) {
        std::cout<<"\n"<<utility_.toString(curves.at(c).points_.at(p));
      }
    }
  }
  

  // Push 0 onto knot point indices
  res.trajectory.index_knot_points.push_back(0);

  
  uint8_t c=0;
  // Go through every knotpoint in the path
  // (or until timeCutoff has been reached)
  for (i_kp_ = 1; i_kp_<path_.points.size(); i_kp_++) {
    //std::cout<<"\ni_kp: "<<(int)i_kp_<<"\n";
    resultValue = 0;
      
    // Push the initial state onto trajectory
    // And set previous knot point
    if(i_kp_ == 1) {
      res.trajectory.trajectory.points.push_back(buildTrajectoryPoint(*reflexxesData_.inputParameters));
      prevKP_ = res.trajectory.trajectory.points.at(0);
    }


    /** Bezier */
    // If its a Bezier curve traj, and we're at a Bezier point
    // all points between first and last are bezier point
    // TODO: straight-line segments between beziers
    if( (bezier_ && i_kp_ > 1 && i_kp_ < path_.points.size()-1) ||
        (partial_ && i_kp_ > 1 && i_kp_ < 3) ||
        (transition_ && i_kp_ == 1) ) 
    {
      //std::cout<<"\nIn if\n";


      // Insert all points on the curves into the trajectory
      for(uint32_t p=1;p<curves.at(c).points_.size()-1;p++) {
        insertPoint(curves.at(c).points_.at(p), res);

        // If it's the first or last point on the curve, push the index to knot point vector
        if(p==curves.at(c).points_.size()-2) {
          res.trajectory.index_knot_points.push_back(res.trajectory.trajectory.points.size() - 1);
        } // end if knot point
      } // end for
      c++;
    } // end if bezier


    /** Straight Line Segment */
    // Else if not bezier or 1st/last segment with bezier
    // 2nd clause is for not doing the last part if its a transition trj
    //else if(i_kp_ > 1 && i_kp_ < path_.points.size()-1) {
    else {
      //std::cout<<"\nIn else\n";

      // Get rotation if needed
        double trajec_size = res.trajectory.trajectory.points.size();
        //std::cout<<"\ntrajec_size: "<<trajec_size;

        trajectory_msgs::JointTrajectoryPoint last = res.trajectory.trajectory.points.at(trajec_size-1);
        trajectory_msgs::JointTrajectoryPoint next_knot = utility_.getTrajectoryPoint(path_.points.at(i_kp_).motionState);
        /*std::cout<<"\nlast: "<<utility_.toString(last);
        std::cout<<"\nnext_knot: "<<utility_.toString(next_knot);
        std::cout<<"\nutility_.findAngleFromAToB(last, next_knot): "<<utility_.findAngleFromAToB(last, next_knot);
        std::cout<<"\nutility_.findDistanceBetweenAngles(last.positions.at(2), utility_.findAngleFromAToB(last, next_knot)): "<<utility_.findDistanceBetweenAngles(last.positions.at(2), utility_.findAngleFromAToB(last, next_knot))<<"\n";*/


        if(fabs(utility_.findDistanceBetweenAngles(last.positions.at(2), utility_.findAngleFromAToB(last, next_knot))) > 0.173) {
          std::vector<trajectory_msgs::JointTrajectoryPoint> rotate_points = rotate(last.positions.at(2), utility_.findAngleFromAToB(last, next_knot));
          //std::cout<<"\nrotate points size: "<<rotate_points.size();
          for(uint8_t p=0;p<rotate_points.size();p++) {
            //std::cout<<"\nPoint "<<p<<": "<<utility_.toString(rotate_points.at(p));
            res.trajectory.trajectory.points.push_back(rotate_points.at(p));
          }

          setSelectionVector();
          resultValue = 0;
        }


     
      // Set target to next knot point
      setTarget(path_.points.at(i_kp_).motionState);
      //std::cout<<"\nPrev KP: "<<utility_.toString(prevKP_)<<"\n";
      //std::cout<<"\nTarget: "<<utility_.toString(path_.points.at(i_kp_).motionState)<<"\n";



      // We go to the next knotpoint only once we reach this one
      while (!finalStateReached()) {

        trajectory_msgs::JointTrajectoryPoint p = spinOnce();

        // Compute the motion state at t+1 and save it in the trajectory
        res.trajectory.trajectory.points.push_back(p);
      } // end while

      // Once we reached the target, we set that the latest point is a knotpoint
      res.trajectory.index_knot_points.push_back(res.trajectory.trajectory.points.size() - 1);
      
    } // end else




    // Set previous knot point
    prevKP_ = res.trajectory.trajectory.points.at(res.trajectory.trajectory.points.size() - 1);

    //std::cout<<"\nReached target at \n"<<utility_.toString(prevKP_)<<"\n";
    //std::cin.get();
  } // end for

  
  // Lastly, set newPath in case the path changed
  res.newPath = path_;
  //std::cout<<"\nReturning: "<<utility_.toString(res.trajectory)<<"\n";
  //std::cin.get();
  return true;
} // End trajectoryRequest callback



const std::vector<trajectory_msgs::JointTrajectoryPoint> MobileBase::rotate(const double start, const double goal) {
  //std::cout<<"\nIn rotate\n";
  std::vector<trajectory_msgs::JointTrajectoryPoint> result;

  setSelectionVectorRotation();


  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[0] = 0;
  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[1] = 0;
  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[0] = 0;
  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[1] = 0;

  // Set current values for orientation
  reflexxesData_.inputParameters->CurrentPositionVector->VecData[2] = start;
  reflexxesData_.inputParameters->CurrentVelocityVector->VecData[2] = 0;
  reflexxesData_.inputParameters->CurrentAccelerationVector->VecData[2] = 0;

  // Set target values
  reflexxesData_.inputParameters->TargetPositionVector->VecData[2] = goal;
  reflexxesData_.inputParameters->TargetVelocityVector->VecData[2] = 0;
  
  resultValue = 0;
  while(!finalStateReached()) {
    trajectory_msgs::JointTrajectoryPoint p = spinOnce();
    result.push_back(p);
  }

  return result;
}




// Returns true if the target has been reached
bool MobileBase::finalStateReached() {
  return (resultValue == ReflexxesAPI::RML_FINAL_STATE_REACHED);
  //return ((resultValue == ReflexxesAPI::RML_FINAL_STATE_REACHED) || (timeFromStart_ >= timeCutoff_));
}


