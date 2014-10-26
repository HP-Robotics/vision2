#include <cstring>
#include <stdio.h>
#include <sys/time.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "image.h"

/* These are functions that require C++ constructs.
   We try to keep everything else in C, for speed */

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
    gettimeofday(&start, NULL);

    Mat a = cvarrToMat(img);
    Canny(a, a, threshold, threshold * 3.0, 3);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    if (display)
        cvShowImage("Canny", img);
}

void find_contours(IplImage *img, struct timeval *t, int display, int level)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    Mat copy = Mat(img, true);

    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    findContours(copy, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);
printf("Found %ld contours\n", contours.size());

    if (display)
    {
        Mat cnt_img = Mat::zeros(copy.size(), CV_8UC1);
        drawContours( cnt_img, contours, -1, Scalar(128,255,255),
                      3, CV_AA, hierarchy, level);

        imshow("Contours", cnt_img);
    }

    /* Fixme - free the Mat? */
}
