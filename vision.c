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
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/videodev2.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc/imgproc_c.h"

#include "capture.h"
#include "image.h"
#include "socket.h"

//Woodshop filter { 0, 23, 0, 134, 0, 122 }
/*
    int min_y;
    int max_y;
    int min_u;
    int max_u;
    int min_v;
    int max_v;
    int apply_fancy_logic;
*/
filter_t g_color_filter = { 70, 255, 105, 126, 0, 115, 0};

//filter_t g_color_filter = { 0, 255, 0, 255, 0, 100 };
//{ 128, 255, 0, 255, 64, 255 };

/* 1 color for mono, 3 colors for rgb */
int g_colors = 1;

/* Toggles for whether or not to do and display various things */
int g_display = 0;
int g_filter = 1;
int g_blur = 1;
int g_contours = 0;
int g_canny = 1;
int g_sobel = 0;
int g_fast = 0;
int g_hough = 1;

double g_canny_threshold = 10.0;
int g_contour_level = 1;

int g_desired_fps = 1;
int g_desired_width = 640;
int g_desired_height = 480;

int g_snap_next = 0;
int g_snap = 1;

char *g_blur_type = "gaussian";

char *g_listen;
char *g_streaming;
int g_stream_count = 0;

char *g_watching;
char g_watch_this_dir[1024];
int g_watch_count = 0;
time_t g_watch_until = 0;
static void stop_watching(void);

long g_count = 0;

int g_rpm = 3300;

capture_t g_cam;

struct timeval g_total_retrieve_time;
struct timeval g_total_blur_time;
struct timeval g_total_contour_time;
struct timeval g_total_canny_time;
struct timeval g_total_sobel_time;
struct timeval g_total_fast_time;

char g_save_to_fname[PATH_MAX];

typedef struct
{
    unsigned int id;
    char *name;
    int val;
    int minimum;
    int maximum;
    int step;
    int cvmax;
    float multiplier;
} camera_control_t;

typedef struct
{
    char *name;
    int  default_value;
} camera_default_t;

camera_default_t g_camera_defaults[] =
{
     { "Exposure, Auto", 1},
     { "Exposure (Absolute)",0},
     { "Brightness", 30 } ,

};


static void compute_reticle(int *x, int *y)
{
    if (g_rpm == 3300)
    {
        *x = 229;
        *y = 447;
    }
    else if (g_rpm == 3200)
    {
        *x = 221;
        *y = 324;
    }
    else if (g_rpm > 3400)
    {
        *x = 210;
        *y = 163;
    }
}

/* Processing for stream to driver, saving shots */
static void save_images(capture_t *c, void *raw)
{
    IplImage *img = NULL;
    void *data = NULL;
    img = cvCreateImageHeader(cvSize(c->width, c->height),  IPL_DEPTH_8U, g_colors);
    data = calloc(1, c->width * c->height * g_colors);
    capture_yuv_to_rgb(raw, data, c->width, c->height, g_colors, NULL);
    cvSetData(img, data, c->width * g_colors);

    if (g_streaming)
    {
        IplImage *rotated;
        char fname[1024];
        int radius = 20;
        int x, y;

        compute_reticle(&x, &y);
        CvScalar black = cvScalar(0,0,0,0);

        compute_reticle(&x, &y);

        rotated = cvCreateImage(cvSize(img->height, img->width), img->depth, img->nChannels);
        cvTranspose(img, rotated);
        cvFlip(rotated, NULL, 1);

        if (g_watching && time(NULL) < g_watch_until)
        {
            FILE *fp;

            g_watch_count++;

            sprintf(fname, "%s/img%05d.png", g_watch_this_dir, g_watch_count);
            cvSaveImage(fname, rotated, NULL);

            sprintf(fname, "%s/raw/img%05d.yuv.raw", g_watch_this_dir, g_watch_count);
            fp = fopen(fname, "w");
            if (fp)
            {
                fwrite(raw, 1, c->width * c->height * 2, fp);
                fclose(fp);
            }

        }
        else if (g_watching && g_watch_count > 0)
            stop_watching();


        g_stream_count = (g_stream_count + 1) % 20;
        sprintf(fname, "%s/img%03d.jpg", g_streaming, g_stream_count);

        cvCircle(rotated, cvPoint(x,y), radius, black, 2, 8, 0);
        cvLine(rotated, cvPoint(x, y - radius), cvPoint(x, y - radius - radius*2), black, 2,8,0);
        cvLine(rotated, cvPoint(x, y + radius), cvPoint(x, y + radius + radius*2), black, 2,8,0);
        cvLine(rotated, cvPoint(x - radius, y), cvPoint(x - radius - radius * 2, y), black, 2,8,0);
        cvLine(rotated, cvPoint(x + radius, y), cvPoint(x + radius + radius * 2, y), black, 2,8,0);

        cvSaveImage(fname, rotated, NULL);
        cvReleaseImage(&rotated);

        {
            char hackbuf[1024];
            sprintf(hackbuf, "mv %s/img%03d.jpg %s/snapshot.jpg", g_streaming, g_stream_count, g_streaming);
            system(hackbuf);
        }
    }
    /* JPW TODO - Save to /var/www/html/shots for a period of time */
    free(data);
    cvReleaseImageHeader(&img);
}

static IplImage *vision_retrieve(capture_t *c)
{
    void *i;
    void *data;
    IplImage *img = NULL;
    filter_t *filter = g_filter ? &g_color_filter : NULL;

    i = capture_retrieve(c, g_colors, NULL, 1);
    if (i)
    {
        save_images(c, i);

        img = cvCreateImageHeader(cvSize(c->width, c->height),  IPL_DEPTH_8U, g_colors);
        data = calloc(1, c->width * c->height * g_colors);
        if (capture_yuv_to_rgb(i, data, c->width, c->height, g_colors, filter))
        {
            fprintf(stderr, "Unexpected conversion error\n");
            free(i);
            free(data);
            return NULL;
        }
        free(i);

        cvSetData(img, data, c->width * g_colors);
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


static int compute_size(long size, int *width, int *height)
{
    if (size == 640 * 480 * 2)
    {
        *width = 640;
        *height = 480;
        return 0;
    }

    return -1;
}


static void start_watching(void)
{
    char tbuf[1024];
    char *p;
    time_t t = time(NULL);
    mkdir(g_watching, 0755);

    strcpy(tbuf, ctime(&t));
    for (p = tbuf; *p; p++)
        if (*p == ' ')
            *p = '.';
        else if (*p == '\n')
            *p = 0;

    sprintf(g_watch_this_dir, "%s/%04d.%s", g_watching, g_rpm, tbuf);
    mkdir(g_watch_this_dir, 0755);

    sprintf(tbuf, "%s/raw", g_watch_this_dir);
    mkdir(tbuf, 0755);

    g_watch_count = 0;
    g_watch_until = time(NULL) + 3;
}

static void stop_watching(void)
{
    char fname[1024];
    FILE *fp;
    int i;

    sprintf(fname, "%s/index.html", g_watch_this_dir);
    fp = fopen(fname, "w");
    if (fp)
    {
        fprintf(fp, "<html><body><h1>%s</h1>\n", g_watch_this_dir);
        for (i = 1; i <= g_watch_count; i++)
        {
            fprintf(fp, "<p>Image %d:<br><img src=\"img%05d.png\"></p>\n", i, i);
        }
        fprintf(fp, "</body></html>\n");
        fclose(fp);
    }
    g_watch_count = 0;
    g_watch_until = 0;    
}


// Our caller guarantees us a null terminator...
static void report_info(int s, char *buf, int len, void *from, int from_len)
{
    printf("Got '%s'\n", buf);
    if (len > 4 && memcmp(buf, "RPM ", 4) == 0)
        g_rpm = atoi(buf + 4);
    if (g_watching && len >= 5 && memcmp(buf, "WATCH", 5) == 0)
        start_watching();
}

IplImage * vision_from_raw_file(char *filename)
{
    void *raw_data = NULL;
    void *data = NULL;
    IplImage *img = NULL;
    FILE *fp;
    struct stat s;
    int width;
    int height;

    if (stat(filename, &s))
    {
        return NULL;
    }

    fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

    if (compute_size(s.st_size, &width, &height)){
        fprintf(stderr, "Error: size of %ld not understood.\n", s.st_size);
        return NULL;
    }

    raw_data = calloc(1, s.st_size);
    fread(raw_data, 1, s.st_size, fp);
    fclose(fp);

    data = calloc(1, width * height * g_colors);
    if (capture_yuv_to_rgb(raw_data, data, width, height, g_colors, g_filter ? &g_color_filter : NULL))
        return NULL;

    img = cvCreateImageHeader(cvSize(width, height),  IPL_DEPTH_8U, g_colors);
    cvSetData(img, data, width * g_colors);
    free(raw_data);

    return img;
}

void vision_print_yuv(char *filename, int x, int y)
{
    unsigned char *raw_data = NULL;
    unsigned char *p;
    FILE *fp;
    struct stat s;
    int width;
    int height;
    int x2;
    int u;
    int v;
    int y1;
    int y2;
    int cg;
    int cb;
    int cr;
    int g1;
    int g2;
    int b1;
    int b2;
    int r1;
    int r2;


    if (stat(filename, &s))
        return;

    fp = fopen(filename, "rb");
    if (!fp)
        return;

    if (compute_size(s.st_size, &width, &height)){
        fprintf(stderr, "Error: size of %ld not understood.\n", s.st_size);
        return;
    }

    raw_data = calloc(1, s.st_size);
    fread(raw_data, 1, s.st_size, fp);
    fclose(fp);

    x2 = (x / 2);
    p = (raw_data + (y * width * 2));
    p += (x2 * 4);

    u = p[1];
    v = p[3];
    y1 = p[0];
    y2 = p[2];
    cb = ((u - 128) * 454) >> 8;
    b1 = y1 + cb;
    b2 = y2 + cb;

    cr = ((v - 128) * 359) >> 8;
    r1 = y1 + cr;
    r2 = y2 + cr;

    cg = (u - 128) * 88;
    cg = (cg + ((v - 128) * 183)) >> 8;
    g1 = y1 - cg;
    g2 = y2 - cg;

    fprintf(stderr, "yuv for %d, %d: Y ", x, y);
    if (x % 2)
        fprintf(stderr, "Y %d, U %d, V %d, g %d, r %d, b %d\n", y1, u, v, g1, b1, r1);
    else
        fprintf(stderr, "Y %d, U %d, V %d, g %d, r %d, b %d\n", y2, u, v, g2, b2, r2);


    free(raw_data);
}


IplImage * vision_from_normal_file(char *filename)
{
    IplImage *img;
    img = cvLoadImage(filename, CV_LOAD_IMAGE_UNCHANGED);
    if (!img)
        return NULL;

        // TODO - filter...
    return img;
}

static void usage(char *argv0)
{
    printf("%s [--display] [--color [--fps n] [--width n] [--height n] [fname1] [fname2]\n", argv0);
    printf("%*.*s [--blur type] [--canny rate] [--contours xx] [--fast] [--sobel xxx] [--hough xxx]\n", (int) strlen(argv0), (int) strlen(argv0), "");
    printf("%*.*s [--no-filter] [--filter]\n", (int) strlen(argv0), (int) strlen(argv0), "");
    printf("%*.*s [--listen host:port]\n", (int) strlen(argv0), (int) strlen(argv0), "");
}

static int parse_arguments(int argc, char *argv[])
{
    int c;

    static struct option long_options[] =
    {
        {"display", no_argument,       0,  'd' },
        {"color",   no_argument,       0,  'c' },
        {"no-filter", no_argument,     0,  'T' },
        {"filter", no_argument,     0,     't' },
        {"fps",     required_argument, 0,  'f' },
        {"width",   required_argument, 0,  'w' },
        {"height",  required_argument, 0,  'h' },
        {"blur",    required_argument, 0,  'b' },
        {"canny",   required_argument, 0,  'a' },
        {"sobel",   required_argument, 0,  '1' },
        {"contours", required_argument, 0, 'z' },
        {"fast",    no_argument,       0,  's' },
        {"hough", required_argument, 0, '2' },
        {"listen", required_argument, 0, 'l' },
        {"streaming", required_argument, 0, '3' },
        {"watch", required_argument, 0, '4' },
        {"simple",    no_argument,       0,  '0' },
        {0,         0,                 0,  0 }
    };

    while (1)
    {
        int option_index = 0;
        c = getopt_long(argc, argv, "0dcTtf:w:h:b:a:1:z:s:2:3:4:", long_options, &option_index);
        switch(c)
        {
            case 'd':
                g_display = 1;
                break;

            case 'c':
                g_colors = 3;
                break;

            case 'T':
                g_filter = 0;
                break;

            case 't':
                g_filter = 1;
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

            case 'l':
                g_listen = strdup(optarg);
                break;

            case '3':
                g_streaming = strdup(optarg);
                break;

            case '0':
                g_blur = g_filter = g_canny = g_hough = 0;
                break;

            case '4':
                g_watching = strdup(optarg);
                break;

            case 'b':
                g_blur_type = strdup(optarg);
                g_blur = 1;
                break;

            case 'a':
                g_canny_threshold = (double) atoi(optarg);
                g_canny = 1;
                break;
            case '1':
                g_sobel=1;
            case 'z':
                g_contour_level = atoi(optarg);
                g_contours = 1;
                break;

            case 's':
                g_fast = 1;
                break;
			case '2':
				g_hough=1;
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

static void print_stats (long count, struct timeval *r, struct timeval *b, struct timeval *c, struct timeval *can, struct timeval *sob)
{
    printf("Took %ld pictures. [retrieve %g|blur %g|contour %g|canny %g|sobel %g]\n", count,
        print_avg_time(r, count),
        print_avg_time(b, count),
        print_avg_time(c, count),
        print_avg_time(can, count),
        print_avg_time(sob, count));
}

void camera_control_cb(int val, void *arg)
{
    int rc;
    camera_control_t *t = (camera_control_t *) arg;
    int v;

    if (t->minimum >= 0 && t->maximum <= 255)
    {
        v = val;
        if (v < t->minimum)
        {
            cvSetTrackbarPos(t->name, "Toolbar", t->minimum);
            return;
        }
    }
    else
    {
        v = (val * t->multiplier) + t->minimum;
        v -= (v - t->minimum) % t->step;
    }

    rc = capture_set_control(&g_cam, t->id, v);
    if (getenv("DEBUG"))
        printf("Set %s to %d: rc %d\n", t->name, v, rc);
    if (rc != 0)
        fprintf(stderr, "Error:  could not set %s to %d\n", t->name, v);
}

static camera_control_t camera_controls[64];
static int control_count = -1;

static void setup_camera_controls(void)
{
    int id;
    int j;
    struct v4l2_queryctrl ctrl;

    if (control_count < 0)
    {
        control_count = 0;

        for (id = 0; control_count < sizeof(camera_controls) / sizeof(camera_controls[0]); id = ctrl.id)
        {
            camera_control_t *t;
            memset(&ctrl, 0, sizeof(ctrl));
            if (capture_query_control(&g_cam, id | V4L2_CTRL_FLAG_NEXT_CTRL, &ctrl) != 0)
                break;

            if (getenv("DEBUG"))
                printf("ctrl %s; max %d, min %d, step %d, flags 0x%x\n",
                    ctrl.name, ctrl.maximum, ctrl.minimum, ctrl.step, ctrl.flags);

            for (j = 0; j < sizeof(g_camera_defaults) / sizeof(g_camera_defaults[0]); j++)
                if (strcmp(g_camera_defaults[j].name, (char *) ctrl.name) == 0)
                {
                    if (capture_set_control(&g_cam, ctrl.id, g_camera_defaults[j].default_value))
                        fprintf(stderr, "Error setting %s to default %d\n", ctrl.name, 
                                        g_camera_defaults[j].default_value);
                }

            if (ctrl.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_READ_ONLY))
            {
                printf("Skipping %s for now; disabled, or read only.\n", ctrl.name);
                continue;
            }

            t = &camera_controls[control_count++];
            t->name = strdup((char *) ctrl.name);
            t->id = ctrl.id;
            t->minimum = ctrl.minimum;
            t->maximum = ctrl.maximum;
            t->step = ctrl.step;
            t->multiplier = 0.0;

            if (t->minimum >= 0 && t->maximum <= 255)
                t->cvmax = ctrl.maximum;
            else
            {
                if ((ctrl.maximum - ctrl.minimum) / ctrl.step <= 255)
                {
                    t->cvmax = ((ctrl.maximum - ctrl.minimum) / ctrl.step);
                    t->multiplier = ctrl.step;
                }
                else
                {
                    t->cvmax = 255;
                    t->multiplier = (ctrl.maximum - ctrl.minimum) / 255;
                }

            }
        }
    }

}

static void display_camera_controls(void)
{
    int i;
    for (i = 0; i < control_count; i++)
    {
        camera_control_t *t = &camera_controls[i];
        int val;

        capture_get_control(&g_cam, t->id, &val);
        if (t->minimum >= 0 && t->maximum <= 255)
            t->val = val;
        else
            t->val = (val - t->minimum) / t->multiplier;
        printf("%s: displaying value of %d as %d\n", t->name, val, t->val);
        cvCreateTrackbar2(t->name, "Toolbar", &t->val, t->cvmax, &camera_control_cb, t);
    }
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

static void toolbar_window(void)
{
    static int displayed = 0;
    setup_window("Toolbar", g_cam.width, 0, g_display);
    if (displayed != g_display)
    {
        displayed = g_display;
        cvCreateTrackbar("Y Min", "Toolbar", &g_color_filter.min_y, 255, NULL);
        cvCreateTrackbar("Y Max", "Toolbar", &g_color_filter.max_y, 255, NULL);
        cvCreateTrackbar("U Min", "Toolbar", &g_color_filter.min_u, 255, NULL);
        cvCreateTrackbar("U Max", "Toolbar", &g_color_filter.max_u, 255, NULL);
        cvCreateTrackbar("V Min", "Toolbar", &g_color_filter.min_v, 255, NULL);
        cvCreateTrackbar("V Max", "Toolbar", &g_color_filter.max_v, 255, NULL);

        display_camera_controls();
    }
}

static void blur_window(void)
{
    setup_window("Blur", g_cam.width, 0, g_blur && g_display);
}

static void canny_window(void)
{
    setup_window("Canny", 0, g_cam.height, g_canny && g_display);
}
static void sobel_window(void)
{
    setup_window("Sobel", 0, g_cam.height, g_sobel && g_display);
}

static void contour_window(void)
{
    setup_window("Contours", g_cam.width, g_cam.height, g_contours && g_display);
}

static void fast_window(void)
{
    setup_window("Fast", g_cam.width, g_cam.height, g_fast && g_display);
}
static void hough_window(void)
{
	setup_window("Hough", g_cam.width, g_cam.height, g_hough && g_display);
}
static void setup_windows(void)
{
    static int started = 0;

    if (g_display && ! started)
    {
        started = 1;
    }
    toolbar_window();
    main_window();
    blur_window();
    canny_window();
    sobel_window();
    contour_window();
    fast_window();
    hough_window();
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
    if (tcgetattr(0, &t) < 0) {
        static int warned = 0;
        if (! warned)
        {
            perror("tcgetattr");
            warned++;
        }
        return -2;
    }

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
    else if (c == '1')
    {
        g_sobel = !g_sobel;
        printf("sobel %s\n", g_sobel ? "on" : "off");
        sobel_window();
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
    else if(c=='2')
    {
    	g_hough=!g_hough;
    	printf("hough %s\n", g_fast ? "on" : "off");
    	hough_window();
    }
    else if (c == '.')
        print_stats(g_count, &g_total_retrieve_time, &g_total_blur_time, &g_total_contour_time, &g_total_canny_time, &g_total_sobel_time);

    else if (c == 'x')
        g_snap_next = 1;

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

int vision_snapshot_number(void)
{
    if (! g_snap_next)
        return -1;
    return g_snap;
}

char *vision_file_template(int s, char *type, char *ext)
{
    static char buf[PATH_MAX];
    sprintf(buf, "snaps/vision.%04d.%s.%s", s, type, ext);
    return buf;
}


void show_yuv(int event, int x, int y, int flags, void* userdata)
{
    vision_print_yuv((char *) userdata, x, y);
}

void process_one_image(IplImage *img, char *filename)
{

    if (g_display) {
        cvShowImage("Camera", img);
        if (filename)
            cvSetMouseCallback("Camera", show_yuv, filename);
    }

    if (g_snap_next) {
        cvSaveImage(vision_file_template(g_snap, "initial", "png"), img, 0);
        printf("Took snap %d\n", g_snap);
    }

    if (g_blur)
    {
        process_blur(img, g_blur_type, &g_total_blur_time);
        if (g_display)
            cvShowImage("Blur", img);

        if (g_snap_next)
            cvSaveImage(vision_file_template(g_snap, "blur", "png"), img, 0);
    }


    if (g_canny)
        perform_canny(img, &g_total_canny_time, g_canny_threshold, g_display);
    if(g_sobel)
        perform_sobel(img, &g_total_sobel_time, g_display);
    if (g_contours)
        find_contours(img, &g_total_contour_time, g_display, g_contour_level);

    if (g_hough)
        Hough(img, &g_total_contour_time, g_display);

    if (g_fast)
    {
        perform_fast(img, &g_total_fast_time, g_display);
        if (g_display)
            cvShowImage("Fast", img);
        if (g_snap_next)
            cvSaveImage(vision_file_template(g_snap, "fast", "png"), img, 0);
    }

}

int vision_main(int argc, char *argv[])
{
    struct timeval start, end, diff;
    struct timeval main_start;

    int discard = 0;

    mkdir("snaps", 0755);

    if (parse_arguments(argc, argv))
        return -1;

    if (g_listen) {
        if (socket_start(atoi(g_listen), report_info))
            return -1;
    }

    if (optind < argc) {
        int i;

        for (i = optind; i < argc; i++)
        {
            IplImage *in_img;
            IplImage *img;
            printf("%d: %s\n", i, argv[i]);
            if (strlen(argv[i]) >= 3 && strcmp(argv[i] + strlen(argv[i]) - 3, "raw") == 0)
                img = vision_from_raw_file(argv[i]);
            else
            {
                in_img = vision_from_normal_file(argv[i]);
                img = cvCreateImage(cvGetSize(in_img), IPL_DEPTH_8U, 1);
                cvCvtColor(in_img, img, CV_RGB2GRAY);
            }
            if (!img)
            {
                fprintf(stderr, "Error:  cannot read %s\n", argv[i]);
                return -1;
            }
            process_one_image(img, argv[i]);
            cvWaitKey(1);
            usleep(100 * 1000);
        }

        cvWaitKey(0);
        return 0;
    }

    if (capture_start(&g_cam, "/dev/video1", g_desired_width, g_desired_height, g_desired_fps) != 0)
        if (capture_start(&g_cam, "/dev/video0", g_desired_width, g_desired_height, g_desired_fps) != 0)
            return -1;

    printf("Capture started; %dx%d at %d fps. %sdisplaying.  Color depth %d.\n", g_cam.width, g_cam.height, g_cam.fps,
            g_display ? "" : "Not ", g_colors);

    gettimeofday(&main_start, NULL);

    setup_windows();
    setup_camera_controls();

    while (1)
    {
        int c;

        c = getch();
        if (c < 0 && g_display)
        {
            c = cvWaitKey(1);
            if (c != -1)
            {
                c &= 0xFFFF;
                printf("got key 0x%x (%c)\n", c, c);
            }
        }

        if (c == 'q' || c == 27)
            break;

        if (c > 0)
            process_key(c, &g_color_filter);

        gettimeofday(&start, NULL);
        if (capture_grab(&g_cam) > 0)
        {
            IplImage *img;

            img = vision_retrieve(&g_cam);

            gettimeofday(&end, NULL);
            timersub(&end, &start, &diff);
            timeradd(&g_total_retrieve_time, &diff, &g_total_retrieve_time);

            process_one_image(img, NULL);
            vision_release(&g_cam, &img);

            g_count++;

            if (g_snap_next)
            {
                g_snap_next = 0;
                g_snap++;
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

    print_stats(g_count, &g_total_retrieve_time, &g_total_blur_time, &g_total_contour_time, &g_total_canny_time, &g_total_sobel_time);

    return 0;
}
