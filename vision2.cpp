#include <cstring>
#include <stdio.h>
#include <string>
#include <exception>
#include <sstream>
#include <unistd.h>
#include <iostream>
#include <getopt.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "capture.h"

int umin = 105;
int umax = 125;
int vmin = 65;
int vmax = 125;
int ymin = 10;
int ymax = 255;

int g_display = 0;
int g_colors = 1;

int g_desired_fps = 1;
int g_desired_width = 320;
int g_desired_height = 240;

using namespace cv;


static IplImage *vision_retrieve(capture_t *c)
{
    void *i;
    IplImage *img = NULL;
    i = capture_retrieve(c, g_colors, umin, umax, vmin, vmax, ymin, ymax);
    if (i)
    {
        img = cvCreateImageHeader(cvSize(c->width, c->height),  IPL_DEPTH_8U, g_colors);
        cvSetData(img, i, c->width * g_colors);
    }
    else
        fprintf(stderr, "Unexpected retrieve error\n");

    return img;
}

static void vision_release(capture_t *c, IplImage **img)
{
    void *i = (*img)->imageData;
    cvReleaseImageHeader(img);
    free(i);
}


static void usage(char *argv0)
{
    printf("%s [--display] [--color [--fps n] [--width n] [--height n]\n", argv0);
}

static int parse_arguments(int argc, char *argv[])
{
    int c;

    static struct option long_options[] =
    {
        {"display", no_argument,       0,  'd' },
        {"color",   no_argument,       0,  'c' },
        {"fps",     required_argument, 0,  'f' },
        {"width",   required_argument, 0,  'w' },
        {"height",  required_argument, 0,  'h' },
        {0,         0,                 0,  0 }
    };

    while (1)
    {
        int option_index = 0;
        c = getopt_long(argc, argv, "dcf:w:h:", long_options, &option_index);
        switch(c)
        {
            case 'd':
                g_display = 1;
                break;

            case 'c':
                g_colors = 3;
                break;

            case 'f':
                g_desired_fps = atoi(optarg);
                break;

            case 'w':
                g_desired_width = atoi(optarg);
                break;

            case 'h':
                g_desired_height = atoi(optarg);
                break;

            case -1:
                return 0;

            default:
                usage(argv[0]);
                return -1;
        }
    }
}

int main(int argc, char *argv[])
{
    capture_t cam;
    int count = 0;

    if (parse_arguments(argc, argv))
        return -1;

    if (capture_start(&cam, "/dev/video1", g_desired_width, g_desired_height, g_desired_fps) != 0)
        if (capture_start(&cam, "/dev/video0", g_desired_width, g_desired_height, g_desired_fps) != 0)
            return -1;

    printf("Capture started; %dx%d at %d fps. %sdisplaying.  Color depth %d.\n", cam.width, cam.height, cam.fps,
            g_display ? "" : "Not ", g_colors);
    // create a window to display detected faces
    namedWindow("Camera", CV_WINDOW_AUTOSIZE);

    while (1)
    {
        char c = (char) cvWaitKey(1);
        if (c == 'q' || c == 27)
            break;

        if (c == 'u')
            umin += 5;
        if (c == 'j')
            umin -= 5;
        if (c == 'U')
            umax += 5;
        if (c == 'J')
            umax -= 5;

        if (c == 'i')
            vmin += 5;
        if (c == 'k')
            vmin -= 5;
        if (c == 'I')
            vmax += 5;
        if (c == 'K')
            vmax -= 5;

        if (c == 'o')
            ymin += 5;
        if (c == 'l')
            ymin -= 5;
        if (c == 'O')
            ymax += 5;
        if (c == 'L')
            ymax -= 5;

        if (c == 'd')
            g_display = !g_display;

        if (c == 'c')
            g_colors = g_colors ==3 ? 1 : 3;

        if (isalpha(c))
            printf("[u min %d|max %d|v min %d|max %d|y min %d|max %d]\n", umin, umax, vmin, vmax, ymin, ymax);

        if (capture_grab(&cam) > 0)
        {
            IplImage *img;
            img = vision_retrieve(&cam);
            if (g_display)
                cvShowImage("Camera", img);
            vision_release(&cam, &img);
            count++;
        }

    }

    capture_stop(&cam);

    destroyAllWindows();

    printf("Took %d pictures\n", count);

    return 0;
}
