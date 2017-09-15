#ifndef EVALUATE_H
#define EVALUATE_H
#include "ramp_msgs/EvaluationSrv.h"
#include "euclidean_distance.h"
#include "orientation.h"
#include "collision_detection.h"
#include "utility.h"




class Evaluate {
  public:
    Evaluate();

    void perform(ramp_msgs::EvaluationRequest& req, ramp_msgs::EvaluationResponse& res);
    void performFeasibility(ramp_msgs::EvaluationRequest& er);
    void performFeasibilityHmap(ramp_msgs::EvaluationRequest& er);
    void performFitness(ramp_msgs::RampTrajectory& trj, const double& offset, double& result);

    /** Different evaluation criteria */
    EuclideanDistance eucDist_;
    Orientation orientation_;

    ramp_msgs::EvaluationResponse res_;

    CollisionDetection cd_;
    CollisionDetection::QueryResult qr_;
    CollisionDetection::QueryResultPacked qrPacked_;

    //Information sent by the request
    ramp_msgs::RampTrajectory trajectory_;
    std::vector<ramp_msgs::RampTrajectory> ob_trjs_;

    double Q_coll_;
    double Q_kine_;

    bool imminent_collision_;

    double T_norm_;
    double A_norm_;
    double D_norm_;
    double T_weight_;
    double A_weight_;
    double D_weight_;

    std::vector< ros::Duration > t_analy_;
    std::vector< ros::Duration > t_numeric_;

    ramp_msgs::HilbertMap hmap_;
  private:
    Utility utility_;
    bool orientation_infeasible_;
};

#endif
