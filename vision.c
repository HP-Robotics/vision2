/*-------------------------------------------------------------------------
* vision.c - main C code for the vision processing
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <termios.h>
#include <sched.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "capture.h"
#include "image.h"


filter_t g_color_filter = { 117, 128, 0, 255, 0, 255 };

/* 1 color for mono, 3 colors for rgb */
int g_colors = 1;

/* Toggles for whether or not to do and display various things */
int g_display = 0;
int g_filter = 0;
int g_blur = 0;
int g_contours = 0;
int g_canny = 0;
int g_fast = 0;

double g_canny_threshold = 25.0;
int g_contour_level = 1;

int g_desired_fps = 1;
int g_desired_width = 640;
int g_desired_height = 480;

int g_snap = 0;

char *g_blur_type = "gaussian";

long g_count = 0;

capture_t g_cam;

struct timeval g_total_retrieve_time;
struct timeval g_total_blur_time;
struct timeval g_total_contour_time;
struct timeval g_total_canny_time;
struct timeval g_total_fast_time;

char g_save_to_fname[PATH_MAX];

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
    printf("%*.*s [--blur type] [--canny rate] [--contours xx] [--fast]\n", (int) strlen(argv0), (int) strlen(argv0), "");
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
        {"canny",   required_argument, 0,  'a' },
        {"contours", required_argument, 0, 'z' },
        {"fast",    no_argument,       0,  's' },
        {0,         0,                 0,  0 }
    };

    while (1)
    {
        int option_index = 0;
        c = getopt_long(argc, argv, "dcf:w:h:b:a:z:s", long_options, &option_index);
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
                g_blur = 1;
                break;

            case 'a':
                g_canny_threshold = (double) atoi(optarg);
                g_canny = 1;
                break;

            case 'z':
                g_contour_level = atoi(optarg);
                g_contours = 1;
                break;

            case 's':
                g_fast = 1;
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

static void setup_window(char *name, int x, int y, int show)
{
    if (show)
    {
        cvNamedWindow(name, CV_WINDOW_AUTOSIZE);
        cvMoveWindow(name, x, y);
    }
    else
        cvDestroyWindow(name);
}

static void main_window(void)
{
    setup_window("Camera", 0, 0, g_display);
}

static void blur_window(void)
{
    setup_window("Blur", g_cam.width, 0, g_blur && g_display);
}

static void canny_window(void)
{
    setup_window("Canny", 0, g_cam.height, g_canny && g_display);
}

static void contour_window(void)
{
    setup_window("Contours", g_cam.width, g_cam.height, g_contours && g_display);
}

static void fast_window(void)
{
    setup_window("Fast", g_cam.width, g_cam.height, g_fast && g_display);
}

static void setup_windows(void)
{
    static int started = 0;
    
    if (g_display && ! started)
    {
        cvStartWindowThread();
        started = 1;
    }
    main_window();
    blur_window();
    canny_window();
    contour_window();
    fast_window();
}

static void key_usage(int c)
{
    printf("bad key %d[0x%x](%c)\n", c, c,c);//format strings?
    printf("basic controls:\n");
    printf("d - Toggle display on/off\n");
    printf("c - Toggle color mode on/off\n");
    printf("f - Toggle filter on/off\n");
    printf("a - Toggle Canny on/off\n");
    printf("b - Toggle blur on/off\n");
    printf("z - Toggle contours on/off\n");
    printf("s - Toggle fast on/off\n");
    printf("x - take a picture\n");
    printf(". - Print current stats\n");
    printf("space - (display only) pause\n");
    printf("@ - Toggle gpio 10 (light)\n");
    printf("filter adjustment:\n");
    printf("  u/j +/- min_u    U/J +/- max_u\n");
    printf("  i/k +/- min_v    I/K +/- max_v\n");
    printf("  o/l +/- min_y    O/L +/- max_y\n");
    printf("  </> +/- canny    \n");
}

static void set_light(int light)
{
    char buf[50];
    sprintf(buf,"echo %d >> /sys/class/gpio/gpio10/value",light);
    system(buf);
}

static int getch(void)
{
    struct termios t = { 0 };
    unsigned char c;

    fflush(stdout);
    if (tcgetattr(0, &t) < 0)
        return -2;

    t.c_lflag &= ~(ICANON|ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &t) < 0)
        return -2;

    if (read(0, &c, 1) <= 0)
        return -1;

    t.c_lflag |= ICANON|ECHO;
    tcsetattr(0, TCSANOW, &t);

    return c;
}

static inline void process_key(int c, filter_t *filter)
{
    static int light = 1;

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

    else if (c == '<')
        g_canny_threshold -= 1.0;
    else if (c == '>')
        g_canny_threshold += 1.0;

    else if (c == 'd')
    {
        g_display = !g_display;
        printf("display %s\n", g_display ? "on" : "off");
        if (g_display)
            setup_windows();
        else
        {
            setup_windows();
            cvDestroyAllWindows();
        }
    }

    else if (c == 'c')
    {
        g_colors = g_colors == 3 ? 1 : 3;
        printf("color %s\n", g_colors == 3 ? "on" : "off");
    }

    else if (c == 'f')
    {
        g_filter = !g_filter;
        printf("filter %s\n", g_filter ? "on" : "off");
    }

    else if (c == 'a')
    {
        g_canny = !g_canny;
        printf("canny %s\n", g_canny ? "on" : "off");
        canny_window();
    }

    else if (c == 'b')
    {
        if (g_blur)
            g_blur = 0;
        else
            g_blur = 1;
        blur_window();

        printf("blur %s\n", g_blur ? g_blur_type : "off");
    }

    else if (c == 'z')
    {
        g_contours = !g_contours;
        printf("contours %s\n", g_contours ? "on" : "off");
        contour_window();
    }

    else if (c == 's')
    {
        g_fast = !g_fast;
        printf("fast %s\n", g_fast ? "on" : "off");
        fast_window();
    }
    else if (c == '.')
        print_stats(g_count, &g_total_retrieve_time, &g_total_blur_time, &g_total_contour_time, &g_total_canny_time);

    else if (c == 'x')
        g_snap = 1;

    else if (c == ' ')
        cvWaitKey(0);

    else if (c == '@')
    {
        light = !light;
        set_light(light);
    }

    else
        key_usage(c);

    if (strchr("uUjJiIkKoOlL", c))
        printf("Filter: [%03d<--U-->%03d|%03d<--V-->%03d|%03d<--Y-->%03d]\n",
               filter->min_u, filter->max_u,
               filter->min_v, filter->max_v,
               filter->min_y, filter->max_y);
    if (strchr("<>", c))
        printf("Canny: %g\n", g_canny_threshold);

}

int vision_main(int argc, char *argv[])
{
    struct timeval start, end, diff;
    struct timeval main_start;

    int discard = 0;
    int snap = 1;
    char fname[256];

    if (parse_arguments(argc, argv))
        return -1;

    system("echo 10 >> /sys/class/gpio/export"); //light start
    usleep(1000);
    system("echo out >> /sys/class/gpio/gpio10/direction");

    if (capture_start(&g_cam, "/dev/video1", g_desired_width, g_desired_height, g_desired_fps) != 0)
        if (capture_start(&g_cam, "/dev/video0", g_desired_width, g_desired_height, g_desired_fps) != 0)
            return -1;

    printf("Capture started; %dx%d at %d fps. %sdisplaying.  Color depth %d.\n", g_cam.width, g_cam.height, g_cam.fps,
            g_display ? "" : "Not ", g_colors);

    gettimeofday(&main_start, NULL);

    setup_windows();

    while (1)
    {
        int c;

        c = getch();
        if (c < 0 && g_display)
            c = cvWaitKey(1);

        if (c == 'q' || c == 27)
            break;

        process_key(c, &g_color_filter);
        if (g_snap)
            sprintf(g_save_to_fname, "camera.yuyv.%03d", snap);
        else
            g_save_to_fname[0] = 0;

        gettimeofday(&start, NULL);
        if (capture_grab(&g_cam) > 0)
        {
            IplImage *img;

            img = vision_retrieve(&g_cam);

            gettimeofday(&end, NULL);
            timersub(&end, &start, &diff);
            timeradd(&g_total_retrieve_time, &diff, &g_total_retrieve_time);

            if (g_display)
                cvShowImage("Camera", img);

            if (g_snap)
            {
                sprintf(fname, "camera.raw.%03d.png", snap);
                cvSaveImage(fname, img, 0);
            }

            if (g_blur)
            {
                process_blur(img, g_blur_type, &g_total_blur_time);
                if (g_display)
                    cvShowImage("Blur", img);
            }


            if (g_canny)
                perform_canny(img, &g_total_canny_time, g_canny_threshold, g_display);

            if (g_contours)
                find_contours(img, &g_total_contour_time, g_display, g_contour_level);

            if (g_fast)
            {
                perform_fast(img, &g_total_fast_time, g_display);
                if (g_display)
                    cvShowImage("Fast", img);
            }

            vision_release(&g_cam, &img);

            g_count++;

            if (g_snap)
            {
                g_snap = 0;
                snap++;
            }

            discard += capture_clear(&g_cam, NULL, 3);
        }
        else
            sched_yield();
    }

    gettimeofday(&end, NULL);
    timersub(&end, &main_start, &diff);
    printf("Overall fps %g in %ld.%ld, discarded %d\n", g_count / (diff.tv_sec + (diff.tv_usec / 1000000.0)),
        diff.tv_sec, diff.tv_usec, discard);
    printf("fps (with discards) %g\n", (g_count + discard) / (diff.tv_sec + (diff.tv_usec / 1000000.0)));

    capture_stop(&g_cam);

    cvDestroyAllWindows();

    print_stats(g_count, &g_total_retrieve_time, &g_total_blur_time, &g_total_contour_time, &g_total_canny_time);

    return 0;
}
