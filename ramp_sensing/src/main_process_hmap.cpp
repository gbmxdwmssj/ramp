#include<iostream>
#include<ros/ros.h>
#include<nav_msgs/OccupancyGrid.h>
#include<visualization_msgs/MarkerArray.h>
#include "utility.h"
#include "GridMap2D.h"
#include <vector>
#include "circle_packer.h"
#include "ramp_msgs/HilbertMap.h"

using namespace std;
using namespace cv;

Mat hmap_mat;
Mat hmap_edges;


ros::Publisher pub_rviz;
visualization_msgs::MarkerArray inner_radii, outer_radii;


void CannyThreshold(int, void*)
{
  /// Reduce noise with a kernel 3x3
  blur( hmap_mat, hmap_edges, Size(3,3) );
  
  imshow("after blur src", hmap_edges);
  waitKey(0);


  // Somehow, lowThreshold is being converted to unsigned int before this point
  // its value is 32767 (-1 for unsigned 4-byte int)
  // Set the value back to 0 for edge detection to work
  int lowThreshold = 0;
  int ratio = 3;
  int kernel_size = 3;

  /// Canny detector
  Canny( hmap_edges, hmap_edges, lowThreshold, lowThreshold*ratio, kernel_size );

  imshow( "hmap_edges", hmap_edges);
  waitKey(0);
}



vector<cv::Point> bhContoursCenter(const vector<vector< cv::Point> >& contours,bool centerOfMass,int contourIdx=-1)
{
  ROS_INFO("In bhContoursCenter");
  ROS_INFO("centerOfMass: %s contourIdx: %i", centerOfMass ? "True" : "False", contourIdx);
  vector<cv::Point> result;
  if (contourIdx > -1)
  {
      if (centerOfMass)
      {
          Moments m = moments(contours[contourIdx],true);
          result.push_back( cv::Point(m.m10/m.m00, m.m01/m.m00));
      }
      else 
      {
          Rect rct = boundingRect(contours[contourIdx]);
          result.push_back( cv::Point(rct.x + rct.width / 2 ,rct.y + rct.height / 2));
      }
  }
  else 
  {
      if (centerOfMass)
      {
        for (int i=0; i < contours.size();i++)
        {
          if(contours[i].size() > 2)
          {
            for(int j=0;j<contours[i].size();j++)
            {
              ROS_INFO("Contour point[%i][%i]: (%i,%i)", i, j, contours[i][j].x, contours[i][j].y);
            }
            Moments m = moments(contours[i],true);
            ROS_INFO("m.m00: %f m.m10: %f m.m01: %f", m.m00, m.m10, m.m01);
            result.push_back( cv::Point(m.m10/m.m00, m.m01/m.m00));
          }
        }
      }
      else 
      {
        for (int i=0; i < contours.size(); i++)
        {
          Rect rct = boundingRect(contours[i]);
          result.push_back(cv::Point(rct.x + rct.width / 2 ,rct.y + rct.height / 2));
        }
      }
  }

  return result;
}


vector<cv::Point> bhFindLocalMaximum(InputArray _src,int neighbor=2)
{
  // Get image
  Mat src = _src.getMat();

  Mat peak_img = src.clone();

  // Dilate
  dilate(peak_img,peak_img,Mat(),cv::Point(-1,-1),neighbor);
  imshow("dilate", peak_img);
  peak_img = peak_img - src;
  resize(peak_img, peak_img, Size(peak_img.cols*4, peak_img.rows*4));
  imshow("dilate-src", peak_img);

  
  waitKey(0);

  Mat flat_img;
  erode(src,flat_img,Mat(),cv::Point(-1,-1),neighbor);
  imshow("erode", flat_img);
  flat_img = src - flat_img;
  imshow("erode-src", flat_img);

  waitKey(0);


  threshold(peak_img,peak_img,0,255,CV_THRESH_BINARY);
  threshold(flat_img,flat_img,0,255,CV_THRESH_BINARY);
  bitwise_not(flat_img,flat_img);

  imshow("thr_dil", peak_img);
  imshow("thr_ero", flat_img);
  waitKey(0);
  
  printf("\nDone threshold\n");

  // Set 255 to any pixels that flat_img has nonzero value
  peak_img.setTo(cv::Scalar::all(255),flat_img);
  printf("\nDone setTo\n");
  // not operator b/c findContours looks for high-valued regions
  bitwise_not(peak_img,peak_img);
  printf("\nDone bitwise_not\n");

  std::vector<std::vector<cv::Point>> contours;
  findContours(peak_img,contours,CV_RETR_EXTERNAL,CV_CHAIN_APPROX_SIMPLE);
  
   /*
    *  Draw contours
    */
  std::vector<Vec4i> hierarchy;
  RNG rng(12345);
  Mat drawing = Mat::zeros( peak_img.size(), CV_8UC3 );
  for( int i = 0; i< contours.size(); i++ )
  {
    Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
    drawContours( drawing, contours, i, color, 2, 8, hierarchy, 0, cv::Point() );
  }
  // Done drawing contours

  // Get centroids of the contours
  vector<cv::Point> maxima = bhContoursCenter(contours, true);

  /// Show in a window
  namedWindow( "Contours", CV_WINDOW_AUTOSIZE );
  imshow( "Contours", drawing );

  return bhContoursCenter(contours,true);
}


void thresholdHilbertMap(Mat hmap, Mat& result)
{
  threshold(hmap, result, 51, 255, CV_THRESH_BINARY);
  imshow("threshold", result);
  waitKey(0);
}


visualization_msgs::Marker getMarker(Circle cir, int id)
{
  visualization_msgs::Marker result;

  result.header.stamp = ros::Time::now();
  result.header.frame_id = "map";
      
  result.ns = "basic_shapes";
  result.id = id;
  
  result.type = visualization_msgs::Marker::SPHERE;
  result.action = visualization_msgs::Marker::ADD;

  // Set x and y
  double x = cir.center.x;
  double y = cir.center.y;

  result.pose.position.x = x;
  result.pose.position.y = y;
  result.pose.position.z = 0;
  result.pose.orientation.x = 0.0;
  result.pose.orientation.y = 0.0;
  result.pose.orientation.z = 0.0;
  result.pose.orientation.w = 1.0;
      
  double radius = cir.radius;
  //ROS_INFO("result info x: %f y: %f radius: %f", x, y, radius);
  
  /*obs[i].cir_.center.x = x;
  obs[i].cir_.center.y = y;
  obs[i].cir_.radius = radius;*/
  
  // scale values are the diameter so use the radius*2
  result.scale.x = radius*2.00f;
  result.scale.y = radius*2.00f;
  result.scale.z = 0.1;
  result.color.r = 0;
  result.color.g = 1;
  result.color.b = 0;
  result.color.a = 1;
  result.lifetime = ros::Duration(10);


  return result;
}

void hmapCb(const ramp_msgs::HilbertMap& hmap)
{
  printf("\nIn hmapCb\n");

  gridmap_2d::GridMap2D hmap_gmap(hmap.map, false); 
  
  // Create cv::Mat  
  hmap_mat = hmap_gmap.probMap();
  cv::transpose(hmap_mat, hmap_mat);
  imshow("hmap", hmap_mat);

  Mat hmap_thresh;

  thresholdHilbertMap(hmap_mat, hmap_thresh);

  CirclePacker cp(hmap_thresh);
  std::vector<Circle> obs = cp.goMyBlobs(true);
  ROS_INFO("obs.size(): %i", (int)obs.size());
  ROS_INFO("hmap origin: (%f,%f) resolution: %f", hmap.map.info.origin.position.x, hmap.map.info.origin.position.y, hmap.map.info.resolution);
  
  double x_origin = hmap.map.info.origin.position.x / hmap.map.info.resolution;
  double y_origin = hmap.map.info.origin.position.y / hmap.map.info.resolution;
  double gamma = hmap.gamma;
  ROS_INFO("x_origin: %f y_origin: %f", x_origin, y_origin);
  for(int i=0;i<obs.size();i++)
  {
    ROS_INFO("Before Obstacle %i: Center - (%f,%f) Radius - %f", i, obs[i].center.x, obs[i].center.y, obs[i].radius);
    resize(hmap_thresh, hmap_thresh, Size(hmap_thresh.cols*4, hmap_thresh.rows*4));
    hmap_thresh.at<uchar>(obs[i].center.x, obs[i].center.y) = 128;
    imshow("center", hmap_thresh);
    waitKey(0);

    obs[i].center.x = (obs[i].center.x * hmap.map.info.resolution) + hmap.map.info.origin.position.x;
    obs[i].center.y = (obs[i].center.y * hmap.map.info.resolution) + hmap.map.info.origin.position.y;
    obs[i].radius *= hmap.map.info.resolution;
    ROS_INFO("After Obstacle %i: Center - (%f,%f) Radius - %f", i, obs[i].center.x, obs[i].center.y, obs[i].radius);
    inner_radii.markers.push_back(getMarker(obs[i], i+obs.size()));

    // Make radius bigger and get new circle
    obs[i].radius += (gamma * obs[i].radius);
    outer_radii.markers.push_back(getMarker(obs[i], i));
  }

  for(int i=0;i<inner_radii.markers.size();i++)
  {
    inner_radii.markers[i].color.r = 255;
    inner_radii.markers[i].color.g = 0;
    inner_radii.markers[i].color.b = 0;
    inner_radii.markers[i].pose.position.z += 0.01;
  }

  pub_rviz.publish(outer_radii);
  pub_rviz.publish(inner_radii);
  

  /*double minV = 0;
  double maxV = 100;
  cv::Point min, max;
  minMaxLoc(hmap_mat, &minV, &maxV, &min, &max);
  ROS_INFO("min: (%i,%i)", min.x, min.y);
  ROS_INFO("max: (%i,%i)", max.x, max.y);*/

  /*
   * Dilation method
   * Dilate with 0 in middle of kernel, then compare image pixel > dilated image pixel
   * Needs peaks to be sharp, can't find peak when gradient is too smooth
   */
  // Create kernel
  /*int kernalSize = 3;
  Mat1b kernelLM(Size(kernalSize, kernalSize), 1);
  kernelLM.at<uchar>(kernalSize/2, kernalSize/2) = 0;
  Mat imageLM;

  // Gaussian blur the image
  //GaussianBlur(hmap_mat, hmap_mat, Size(3,3), 0, 0);
  //imshow("blurred", hmap_mat);

  dilate(hmap_mat, imageLM, kernelLM);
  imshow("dilated",imageLM);
  Mat1b localMaxima = (hmap_mat > imageLM);
  for(int i=25;i<35;i++)
    for(int j=30;j<40;j++)
      ROS_INFO("hmap_mat[%i][%i]: %i", i, j, hmap_mat.at<uchar>(i,j));
  imshow("localMax", localMaxima);
  waitKey(0);

  std::vector<cv::Point> locations;   // output, locations of non-zero pixels
  cv::findNonZero(localMaxima, locations);
  for(int i=0;i<locations.size();i++)
  {
    cv::Point pnt = locations[i];
    ROS_INFO("Local maxima %i: (%i,%i)", i, pnt.x, pnt.y);
    hmap_mat.at<uchar>(pnt.x, pnt.y) = 255;
  }
  imshow("maxima", hmap_mat);
  waitKey(0);*/



  /*
   * Contour method
   * Dilate+Erode and subtract from original to get boundaries, then get contours and use centroid point
   * It finds the region that dilation can't get, but it doesn't get the corner point
   * cannot find boundary of corner point
   */
  /*vector<cv::Point> locations = bhFindLocalMaximum(hmap_mat, 10);
  for(int i=0;i<locations.size();i++)
  {
    cv::Point pnt = locations[i];
    ROS_INFO("Local maxima %i: (%i,%i)", i, pnt.x, pnt.y);
    hmap_mat.at<uchar>(pnt.y, pnt.x) = 255;
  }
  imshow("hmap", hmap_mat);
  waitKey(0);*/


  


  //hmap_mat.at<uchar>(min.y,min.x) = 255;
  //hmap_mat.at<uchar>(max.y,max.x) = 255;
  //imshow("Local Maxima", localMaxima);
  //waitKey(0);
  //getHist();
  //CannyThreshold(0,0);
}


int main(int argc, char** argv)
{
  ros::init(argc, argv, "process_hmap");
  ros::NodeHandle handle;

  ros::Subscriber sub_hmap = handle.subscribe("/hilbert_map", 10, &hmapCb);
  pub_rviz = handle.advertise<visualization_msgs::MarkerArray>("visualization_marker_array", 1);
  
  printf("\nSpinning\n");
  ros::spin();

  printf("\nExiting normally\n");
}
