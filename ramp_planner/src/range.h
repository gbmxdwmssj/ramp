#ifndef RANGE_H
#define RANGE_H
#include "ramp_msgs/Range.h"
#include "utility.h"

class Range {
  public:
    Range();
    Range(const float min, const float max);
    ~Range();

    float min_;
    float max_;

    /** This method returns a random value in the range */
    const float random();
    const ramp_msgs::Range buildRangeMsg() const;

};

#endif
