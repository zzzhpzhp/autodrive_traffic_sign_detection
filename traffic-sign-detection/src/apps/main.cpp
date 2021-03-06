
// By downloading, copying, installing or using the software you agree to this license.
// If you do not agree to this license, do not download, install,
// copy or use the software.


//                           License Agreement
//                For Open Source Computer Vision Library
//                        (3-clause BSD License)

// Copyright (C) 2015,
// 	  Guillaume Lemaitre (g.lemaitre58@gmail.com),
// 	  Johan Massich (mailsik@gmail.com),
// 	  Gerard Bahi (zomeck@gmail.com),
// 	  Yohan Fougerolle (Yohan.Fougerolle@u-bourgogne.fr).
// Third party copyrights are property of their respective owners.

// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:

//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.

//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.

//   * Neither the names of the copyright holders nor the names of the contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.

// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall copyright holders or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.

// our own code
#include <img_processing/segmentation.h>
#include <img_processing/colorConversion.h>
#include <img_processing/imageProcessing.h>
#include <img_processing/contour.h>
#include <optimization/smartOptimisation.h>
#include <common/math_utils.h>
#include <common/timer.h>

// stl library
#include <string>
#include <iostream>
#include <chrono>
#include <ctime>
#include <limits>
#include <utility>

// OpenCV library
#include <opencv2/opencv.hpp>

// Eigen library
#include <Eigen/Core>


struct shapeTracker{
  std::vector < cv::Point > contour;
  std::vector < cv::Point > edges;
  int counter = 0;
  bool incremented = false;

  shapeTracker(std::vector < cv::Point > aEdges,
    std::vector < cv::Point > aContour) :
    edges(aEdges), contour(aContour){}

  void setIncrementedCounter(int count) {
    counter = count + 1;
  }

  int getCount() {
    return counter;
  }

  std::vector < cv::Point > getContour() {
    return contour;
  }


  bool operator==(const shapeTracker rhs) {

    cv::Point currPoint = edges[0];
    cv::Point currPoint1 = edges[1];

    cv::Point prevPoint = rhs.edges[0];
    cv::Point prevPoint1 = rhs.edges[1];

    prevPoint.x = prevPoint.x - 10;
    prevPoint.y = prevPoint.y - 10;
    prevPoint1.x = prevPoint1.x + 10;
    prevPoint1.y = prevPoint1.y + 10;

    if (currPoint.x > prevPoint.x && currPoint.x < prevPoint1.x){
      if (currPoint.y > prevPoint.y && currPoint.y < prevPoint1.y){
        return true;
      }
    } else if (currPoint1.x > prevPoint.x && currPoint1.x < prevPoint1.x){
      if (currPoint1.y > prevPoint.y && currPoint1.y < prevPoint1.y){
        return true;
      }
    } else if (currPoint.x > prevPoint.x && currPoint.x < prevPoint1.x){
      if (currPoint1.y > prevPoint.y && currPoint1.y < prevPoint1.y){
        return true;
      }
    } else if (currPoint1.x > prevPoint.x && currPoint1.x < prevPoint1.x){
      if (currPoint.y > prevPoint.y && currPoint.y < prevPoint1.y){
        return true;
      }
    }

    return false;

  }

};


//TODO: code here should be abstracted outside the app, modify tests accordingly
int main(int argc, char *argv[]) {

    // Chec the number of arguments
    if (argc != 2) {
        std::cout << "********************************" << std::endl;
        std::cout << "Usage of the code: ./traffic-sign-detection imageFileName.extension" << std::endl;
        std::cout << "********************************" << std::endl;

        return -1;
    }

    // Clock for measuring the elapsed time
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

    // Read the input image - convert char* to string
    std::string input_filename(argv[1]);

    // Read the input image
    cv::VideoCapture cap(input_filename);
    cv::Mat input_image;

    // Check that the image has been opened
    if (!cap.isOpened()) {
        std::cout << "File could not be loaded" << std::endl;
        return -1;
    }

    std::vector< shapeTracker > current_corners;
    std::vector< shapeTracker > prev_corners;
    std::vector< std::vector< cv::Point > > contoursToDraw;
    std::vector< std::vector< cv::Point > > temp_contours;
    std::vector< std::vector< cv::Point > > contours;
    while(true){
      // Check that the image read is a 3 channels image
      cap >> input_image;
      if (input_image.data == NULL){
        break;
      }
      CV_Assert(input_image.channels() == 3);


      /*
     * Conversion of the image in some specific color space
     */

     //Convert input image to HSV
     cv::Mat hsv_image;
     cv::cvtColor(input_image, hsv_image, cv::COLOR_BGR2HSV);
     // Threshold the HSV image, keep only the red pixels
     cv::Mat lower_red_hue_range;
     cv::Mat upper_red_hue_range;
     cv::inRange(hsv_image, cv::Scalar(0, 100, 100), cv::Scalar(10, 255, 255), lower_red_hue_range);
     cv::inRange(hsv_image, cv::Scalar(160, 100, 100), cv::Scalar(179, 255, 255), upper_red_hue_range);



     // Combine the above two images
     cv::Mat red_hue_image;
     cv::addWeighted(lower_red_hue_range, 1.0, upper_red_hue_range, 1.0, 0.0, red_hue_image);

      // Filter the image using median filtering and morpho math
      cv::Mat bin_image;
      imageprocessing::filter_image(red_hue_image, bin_image);

      cv::GaussianBlur(bin_image, bin_image, cv::Size(9, 9), 0, 0);
      /*
     * Extract candidates (i.e., contours) and remove inconsistent candidates
     */
     imageprocessing::contours_extraction(bin_image, contours);


      // Initialisation of the variables which will be returned after the distortion. These variables are linked with the transformation applied to correct the distortion
      std::vector< cv::Mat > rotation_matrix(contours.size());
      std::vector< cv::Mat > scaling_matrix(contours.size());
      std::vector< cv::Mat > translation_matrix(contours.size());
      for (unsigned int contour_idx = 0; contour_idx < contours.size(); contour_idx++) {
          rotation_matrix[contour_idx] = cv::Mat::eye(3, 3, CV_32F);
          scaling_matrix[contour_idx] = cv::Mat::eye(3, 3, CV_32F);
          translation_matrix[contour_idx] = cv::Mat::eye(3, 3, CV_32F);
      }

      std::vector< cv::Point > approx;
      cv::Mat output_image = input_image.clone();
      cv::Scalar color(255,255,0);
      for (unsigned int i = 0; i < contours.size(); i++){
        cv::approxPolyDP(cv::Mat(contours[i]), approx,
         cv::arcLength(cv::Mat(contours[i]), true) * 0.01, true);

        std::vector< cv::Point > tempStore;
        int minx = 0;
        int miny = 0;
        int maxx = 0;
        int maxy = 0;
        for (int x = 0; x < approx.size(); x++){
          if (approx.size() > 1 && x == 0){
            minx = std::min(approx[x].x, approx[x+1].x);
            miny = std::min(approx[x].y, approx[x+1].y);
            maxx = std::max(approx[x].x, approx[x+1].x);
            maxy = std::max(approx[x].y, approx[x+1].y);
            continue;
          }

          minx = std::min(approx[x].x, minx);
          miny = std::min(approx[x].y, miny);
          maxx = std::max(approx[x].x, maxx);
          maxy = std::max(approx[x].y, maxy);
        }

        cv::Point *point1 = new cv::Point(minx, miny);
        cv::Point *point2 = new cv::Point(maxx, maxy);
        tempStore.push_back(*point1);
        tempStore.push_back(*point2);
        shapeTracker *shapeTrack = new shapeTracker(tempStore, contours[i]);
        current_corners.push_back(*shapeTrack);

      }

      // Filtering
      for (int i = 0; i < prev_corners.size(); i++){
        for (int j = 0; j < current_corners.size(); j++){

          shapeTracker& shape = current_corners[j];
          shapeTracker& prevShape = prev_corners[i];
          if (shape == prevShape) {
            shape.setIncrementedCounter(prevShape.getCount());
            prev_corners.erase(prev_corners.begin() + i);
            i--;
            if (shape.getCount() >= 20) {
              contoursToDraw.push_back(shape.getContour());
              if (shape.getCount() < 21) {
                std::cout << "STOP SIGN DETECTED" << std::endl;
              }
              break;
            }

            break;

          }

        }
      }


      for (int i = 0; i < contoursToDraw.size(); i++){
        std::vector< cv::Point > approxPoints;
        cv::approxPolyDP(cv::Mat(contoursToDraw[i]), approxPoints,
         cv::arcLength(cv::Mat(contoursToDraw[i]), true) * 0.01, true);


         cv::drawContours(output_image, cv::Mat(contoursToDraw[i]), -1, color, 2, 8);
        /*
        if (approxPoints.size() == 8){
          std::cout << "STOP SIGN" << std::endl;
        } else if (approxPoints.size() == 3){
          std::cout << "TRIANGLE SIGN" << std::endl;
          cv::drawContours(output_image, cv::Mat(contoursToDraw[i]), -1, color, 2, 8);
        } else {
          //std::cout << "OTHER TYPES OF SIGNS" << std::endl;
          //std::cout << "EDGES: " << approx.size() << std::endl;
          //cv::drawContours(output_image, cv::Mat(contours[i]), -1, color, 2, 8);
        }
        */
        contoursToDraw.erase(contoursToDraw.begin() + i);
      }

      prev_corners = current_corners;
      current_corners.clear();
      cv::imshow("Window", output_image);
      cv::waitKey(10);
    }
    cv::waitKey(0);

}
