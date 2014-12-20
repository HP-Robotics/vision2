/*-------------------------------------------------------------------------
* image.cpp - These are functions that require C++
*             (Some OpenCV functions are C++ only)
*
* -------------------------------------------------------------------------
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* -------------------------------------------------------------------------*/
#include <cstring>
#include <stdio.h>
#include <sys/time.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "opencv2/features2d/features2d.hpp"

#include "image.h"
#include "vision.h"

using namespace cv;

void process_blur(IplImage *img, char *type, struct timeval *t)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);
    Mat a = cvarrToMat(img);
    GaussianBlur(a, a, Size(3, 3), 1.0);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);
}

void perform_canny(IplImage *img, struct timeval *t, double threshold, int display)
{
    struct timeval start, end, diff;
    int s;
    gettimeofday(&start, NULL);

    Mat a = cvarrToMat(img);
    Canny(a, a, threshold, threshold * 3.0, 3);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    if (display)
        cvShowImage("Canny", img);

    s = vision_snapshot_number();
    if (s >= 0)
        cvSaveImage(vision_file_template(s, "canny", "png"), img, 0);
}

RNG rng(12345);
void find_contours(IplImage *img, struct timeval *t, int display, int level)
{
    struct timeval start, end, diff;
    int s;
    gettimeofday(&start, NULL);

    Mat copy = Mat(img, true);

    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    findContours(copy, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    s = vision_snapshot_number();

    if (display || s >= 0)
    {
        Mat cnt_img = Mat::zeros(copy.size(), CV_8UC3);
        for (size_t i = 0; i < contours.size(); i++)
        {
            Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
            drawContours( cnt_img, contours, i, color, 2, 8, hierarchy, 0, Point() );
        }
        if (display)
            imshow("Contours", cnt_img);

        //if (s >= 0)
        //    cvSaveImage(vision_file_template(s, "canny", "png"), cnt_img, 0);
    }

}

void perform_fast(IplImage *img, struct timeval *t, int display)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    FastFeatureDetector detector(10, true);

    Mat a = cvarrToMat(img);
    vector<KeyPoint> keypoints;
    detector.detect(a, keypoints);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    if (display)
        drawKeypoints(a, keypoints, a, Scalar::all(-1), DrawMatchesFlags::DRAW_OVER_OUTIMG|DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
}
