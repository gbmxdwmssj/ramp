#include "modifier.h"


Modifier::Modifier(const ros::NodeHandle& h) : num_ops(6) {
  h_mod_req_ = new ModificationRequestHandler(h);
}

Modifier::Modifier(const ros::NodeHandle& h, const std::vector<Path> ps) : num_ops(6), paths_(ps) {
  h_mod_req_ = new ModificationRequestHandler(h);
}

Modifier::~Modifier() {
  if(h_mod_req_ != 0) {
    delete h_mod_req_;
    h_mod_req_ = 0;
  }
}



/** This method builds a ModificationRequest srv */
ramp_msgs::ModificationRequest Modifier::buildModificationRequest() {
  ramp_msgs::ModificationRequest result;

  //First, randomly select an operator
  unsigned int op = rand() % num_ops;
  
  //Assign the correct name for the operator
  switch(op) {

    //Insert
    case 0:
      result.request.op = "insert";

      break;

    //Delete
    case 1:
      result.request.op = "delete";
      break;

    //Change
    case 2:
      result.request.op = "change";
      break;

    //Swap
    case 3:
      result.request.op = "swap";
      break;

    //Crossover
    case 4:
      result.request.op = "crossover";
      break;
  }

  //Get a random path to modify
  unsigned int i_1 = rand() % paths_.size();
  result.request.paths.push_back(paths_.at(i_1).buildPathMsg());

  //If crossover, get a second path
  if(op == 4) {
    unsigned int i_2;
    do { i_2 = rand() % paths_.size(); } 
    while (i_1 != i_2);

    result.request.paths.push_back(paths_.at(i_2).buildPathMsg());
  }

  
  return result;
}





/** This method performs all the tasks for path modification */
std::vector<Path> Modifier::perform() {
  std::vector<Path> result;
 
  //Build a modification request srv 
  ramp_msgs::ModificationRequest mr = buildModificationRequest(); 

  //If the request was successful
  if(h_mod_req_->request(mr)) {

    //Push on the modified paths
    for(unsigned int i=0;i<mr.response.mod_paths.size();i++) {
      Path temp(mr.response.mod_paths.at(i));
      result.push_back(temp);
    }
  }
  else {
    //some error handling
  }

  return result;
}