#include <cstring>
#include <sys/time.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "image.h"

/* These are functions that require C++ constructs.
   We try to keep everything else in C, for speed */

using namespace cv;


void process_blur(IplImage *img, char *type, struct timeval *t, int display)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);
    IplImage *copy = cvCloneImage(img);
    Mat a = cvarrToMat(img);
    Mat b = cvarrToMat(copy);
    GaussianBlur(a, b, Size(3, 3), 1.0);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);
    if (display)
        cvShowImage("Blur", copy);
    cvReleaseImage(&copy);
    /* FIXME - release a and b ? */
}

void find_contours(IplImage *img, struct timeval *t, int display)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);
}
