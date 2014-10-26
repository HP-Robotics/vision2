#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "capture.h"
#include "image.h"


filter_t g_color_filter = { 105, 125, 65, 125, 10, 255 };

int g_display = 1;
int g_colors = 1;
int g_filter = 1;
int g_contours = 1;
int g_canny = 1;
int g_hough = 0;
double g_canny_threshold = 25.0;

int g_desired_fps = 1;
int g_desired_width = 640;
int g_desired_height = 480;

char *g_blur_type = "gaussian";

long g_count = 0;

struct timeval g_total_retrieve_time;
struct timeval g_total_blur_time;
struct timeval g_total_contour_time;
struct timeval g_total_canny_time;


static IplImage *vision_retrieve(capture_t *c)
{
    void *i;
    IplImage *img = NULL;
    i = capture_retrieve(c, g_colors, g_filter ? &g_color_filter : NULL);
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
        {"blur",    required_argument, 0,  'b' },
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

            case 'b':
                g_blur_type = strdup(optarg);
                break;

            case -1:
                return 0;

            default:
                usage(argv[0]);
                return -1;
        }
    }
}

static double print_avg_time(struct timeval *t, long count)
{
    double ret = (t->tv_sec + (t->tv_usec / 1000000.0)) / count;
    return ret;
}

static void print_stats (long count, struct timeval *r, struct timeval *b, struct timeval *c, struct timeval *can)
{
    printf("Took %ld pictures. [retrieve %g|blur %g|contour %g|canny %g]\n", count,
        print_avg_time(r, count),
        print_avg_time(b, count),
        print_avg_time(c, count),
        print_avg_time(can, count));
}


static void key_usage(int c)
{
    printf("bad key %d[0x%x](%c)\n", c, c,c);
    printf("basic controls:\n");
    printf("d - Toggle display on/off\n");
    printf("c - Toggle color mode on/off\n");
    printf("f - Toggle filter on/off\n");
    printf("a - Toggle Canny on/off\n");
    printf("h - Toggle Hough on/off\n");
    printf("n - Toggle contours on/off\n");
    printf(". - Print current stats\n");
    printf("filter adjustment:\n");
    printf("  u/j +/- min_u    U/J +/- max_u\n");
    printf("  i/k +/- min_v    I/K +/- max_v\n");
    printf("  o/l +/- min_y    O/L +/- max_y\n");
    printf("  </> +/- canny    [/] +/- hough\n");
}

static inline void process_key(int c, filter_t *filter)
{
    if (c == -1)
        return;

    /* I haven't quite figured out opencv key codes yet.
       They seem to both shift in (e.g. | 0x10000 for shift
       and | 0x40000 for ctrl) and return a key for the shift
       key (e.g. shift key is 0xffe1).  Just strip 'em for now. */
    c &= 0xFF;
    if (c == 0xe1 || c == 0xe2)
        return;

    if (c == 'u')
        filter->min_u += 5;
    else if (c == 'j')
        filter->min_u -= 5;
    else if (c == 'U')
        filter->max_u += 5;
    else if (c == 'J')
        filter->max_u -= 5;

    else if (c == 'i')
        filter->min_v += 5;
    else if (c == 'k')
        filter->min_v -= 5;
    else if (c == 'I')
        filter->max_v += 5;
    else if (c == 'K')
        filter->max_v -= 5;

    else if (c == 'o')
        filter->min_y += 5;
    else if (c == 'l')
        filter->min_y -= 5;
    else if (c == 'O')
        filter->max_y += 5;
    else if (c == 'L')
        filter->max_y -= 5;

    else if (c == 'd')
    {
        g_display = !g_display;
        printf("display %s\n", g_display ? "on" : "off");
    }

    else if (c == 'f')
    {
        g_filter = !g_filter;
        printf("filter %s\n", g_filter ? "on" : "off");
    }

    else if (c == 'c')
    {
        g_colors = g_colors ==3 ? 1 : 3;
        printf("color %s\n", g_colors == 3 ? "on" : "off");
    }

    else if (c == 'a')
    {
        g_canny = !g_canny;
        printf("canny %s\n", g_canny ? "on" : "off");
    }

    else if (c == 'h')
    {
        g_hough = !g_hough;
        printf("hough %s\n", g_hough ? "on" : "off");
    }

    else if (c == 'z')
    {
        g_contours = !g_contours;
        printf("contours %s\n", g_contours ? "on" : "off");
    }
    else if (c == '.')
        print_stats(g_count, &g_total_retrieve_time, &g_total_blur_time, &g_total_contour_time, &g_total_canny_time);

    else if (c == '<')
        g_canny_threshold -= 1.0;
    else if (c == '>')
        g_canny_threshold += 1.0;

    else
        key_usage(c);

    if (strchr("uUjJiIkKoOlL", c))
        printf("Filter: [%03d<--U-->%03d|%03d<--V-->%03d|%03d<--Y-->%03d]\n",
               filter->min_u, filter->max_u,
               filter->min_v, filter->max_v,
               filter->min_y, filter->max_y);
    if (strchr("<>", c))
        printf("Canny: %g\n", g_canny_threshold);

    //if (isalpha(c))
    //    printf("[u min %d|max %d|v min %d|max %d|y min %d|max %d]\n", umin, umax, vmin, vmax, ymin, ymax);
}

int vision_main(int argc, char *argv[])
{
    capture_t cam;

    struct timeval start, end, diff;
    struct timeval main_start;

    int discard = 0;

    if (parse_arguments(argc, argv))
        return -1;

    if (capture_start(&cam, "/dev/video1", g_desired_width, g_desired_height, g_desired_fps) != 0)
        if (capture_start(&cam, "/dev/video0", g_desired_width, g_desired_height, g_desired_fps) != 0)
            return -1;

    printf("Capture started; %dx%d at %d fps. %sdisplaying.  Color depth %d.\n", cam.width, cam.height, cam.fps,
            g_display ? "" : "Not ", g_colors);

    cvNamedWindow("Camera", CV_WINDOW_AUTOSIZE);
    cvMoveWindow("Camera", 0, 0);

    if (g_blur_type)
    {
        cvNamedWindow("Blur", CV_WINDOW_AUTOSIZE);
        cvMoveWindow("Blur", cam.width, 0);
    }
    if (g_contours)
    {
        cvNamedWindow("Contours", CV_WINDOW_AUTOSIZE);
        cvMoveWindow("Contours", 0, cam.height);
    }
    if (g_canny)
    {
        cvNamedWindow("Canny", CV_WINDOW_AUTOSIZE);
        cvMoveWindow("Canny", cam.width, cam.height);
    }

    gettimeofday(&main_start, NULL);

    while (1)
    {
        int c = cvWaitKey(1);
        if (c == 'q' || c == 27)
            break;

        process_key(c, &g_color_filter);

        gettimeofday(&start, NULL);
        if (capture_grab(&cam) > 0)
        {
            IplImage *img;
            img = vision_retrieve(&cam);

            gettimeofday(&end, NULL);
            timersub(&end, &start, &diff);
            timeradd(&g_total_retrieve_time, &diff, &g_total_retrieve_time);

            if (g_display)
                cvShowImage("Camera", img);

            if (g_blur_type)
            {
                process_blur(img, g_blur_type, &g_total_blur_time);
                if (g_display)
                    cvShowImage("Blur", img);
            }

            if (g_canny)
                perform_canny(img, &g_total_canny_time, g_canny_threshold, g_display);

            if (g_contours)
                find_contours(img, &g_total_contour_time, g_display, 0);

            vision_release(&cam, &img);

            g_count++;
        }

        discard += capture_clear(&cam, NULL, 3);

    }

    gettimeofday(&end, NULL);
    timersub(&end, &main_start, &diff);
    printf("Overall fps %g in %ld.%ld, discarded %d\n", g_count / (diff.tv_sec + (diff.tv_usec / 1000000.0)),
        diff.tv_sec, diff.tv_usec, discard);

    capture_stop(&cam);

    cvDestroyAllWindows();

    print_stats(g_count, &g_total_retrieve_time, &g_total_blur_time, &g_total_contour_time, &g_total_canny_time);

    return 0;
}
