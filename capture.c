/*-------------------------------------------------------------------------
* capture.c - Sub routines to grab data from a v4l camera
*             and optionally filter + convert to grayscale.
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "capture.h"
#include "vision.h"

/* convert from 4:2:2 YUYV interlaced to RGB24 */
/* based on ccvt_yuyv_bgr32() from camstream */
#define SAT(c) \
        if (c & (~255)) { if (c < 0) c = 0; else c = 255; }

static void yuyv_to_rgb24 (unsigned char *src, unsigned char *dst, int width, int height, filter_t *filter)
{
    unsigned char *s;
    unsigned char *d;
    int l, c;
    int r, g, b, cr, cg, cb, y1, y2;

    l = height;
    s = src;
    d = dst;

    while (l--)
    {
        c = width >> 1;
        while (c--)
        {
            if (filter &&
                (s[1] < filter->min_u || s[1] > filter->max_u ||
                 s[3] < filter->min_v || s[3] > filter->max_v))
            {
                d += 6;
                s += 4;
                continue;
            }

            y1 = *s++;
            cb = ((*s - 128) * 454) >> 8;
            cg = (*s++ - 128) * 88;
            y2 = *s++;
            cr = ((*s - 128) * 359) >> 8;
            cg = (cg + (*s++ - 128) * 183) >> 8;

            if (filter && (y1 < filter->min_y || y1 > filter->max_y))
                d += 3;
            else
            {
                r = y1 + cr;
                b = y1 + cb;
                g = y1 - cg;
                SAT(r);
                SAT(g);
                SAT(b);

                *d++ = b;
                *d++ = g;
                *d++ = r;
            }

            if (filter && (y2 < filter->min_y || y2 > filter->max_y))
                d += 3;
            else
            {
                r = y2 + cr;
                b = y2 + cb;
                g = y2 - cg;
                SAT(r);
                SAT(g);
                SAT(b);

                *d++ = b;
                *d++ = g;
                *d++ = r;
            }
        }
    }
}

static void yuyv_to_8(void *in, void *out, int width, int height, filter_t *filter)
{
    unsigned char *p = in;
    unsigned char *q = out;
    int w, h;
    for (h = 0; h < height; h++)
        for (w = 0; w < width; w += 2, p += 4, q += 2)
        {
#if 0
int hack = p[0] + ((88 * p[1]) / 256) + ((183 * p[3]) / 256);
if (hack < 255 || hack > 290)
    continue;
#endif

            if (filter &&
                (p[1] < filter->min_u || p[1] > filter->max_u ||
                 p[3] < filter->min_v || p[3] > filter->max_v))
                continue;

#if  0
hack = p[2] + ((88 * p[1]) / 256) + ((183 * p[3]) / 256);
if (hack < 255 || hack > 290)
    continue;
#endif

            if (!filter || (p[0] >= filter->min_y && p[0] <= filter->max_y))
                *q = p[0];
            if (!filter || (p[2] >= filter->min_y && p[2] <= filter->max_y))
                *(q + 1) = p[2];
        }
}

static int xioctl(int fd, int request, void *arg)
{
    int r;

    do
    {
        r = ioctl(fd, request, arg);
    }
    while (r == -1 && errno == EINTR);

    return r;
}

static int open_device(const char *name)
{
    struct stat st;
    int fd;

    if (stat(name, &st) == -1)
    {
        fprintf(stderr, "%s not found\n", name);
        return -1;
    }

    if (!S_ISCHR(st.st_mode))
    {
        fprintf(stderr, "%s is no device\n", name);
        return -1;
    }

    fd = open(name, O_RDWR /* required */ | O_NONBLOCK, 0);
    if (fd == -1)
    {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
            name, errno, strerror(errno));
        return -1;
    }

    return fd;
}


static int init_device(capture_t *c, int width, int height, int fps)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_streamparm setfps;

    if (xioctl(c->fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        if (errno == EINVAL)
            fprintf(stderr, "Error: not a V4L2 device\n");
        else
            fprintf(stderr, "Error %s(%d): could not query cap\n", strerror(errno), errno);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "Error: not video capture device\n");
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "Error: does not support streaming\n");
        return -1;
    }


    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(c->fd, VIDIOC_G_FMT, &fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (xioctl(c->fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        fprintf(stderr, "Error: could not establish format.\n");
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(c->fd, VIDIOC_G_FMT, &fmt) != -1)
    {
        c->width = fmt.fmt.pix.width;
        c->height = fmt.fmt.pix.height;
    }

    memset (&setfps, 0, sizeof(struct v4l2_streamparm));
    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(c->fd, VIDIOC_G_PARM, &setfps);
    c->fps = fps;

    setfps.parm.capture.timeperframe.numerator = 1;
    setfps.parm.capture.timeperframe.denominator = c->fps;
    if (xioctl(c->fd, VIDIOC_S_PARM, &setfps) == -1)
        fprintf(stderr, "Error %s(%d) changing FPS\n", strerror(errno), errno);

    memset (&setfps, 0, sizeof(struct v4l2_streamparm));
    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(c->fd, VIDIOC_G_PARM, &setfps) == -1)
        fprintf(stderr, "Error %s(%d) getting FPS\n", strerror(errno), errno);
    else
        c->fps = setfps.parm.capture.timeperframe.denominator;

    return 0;
}

static int init_mmap(capture_t *c)
{
    struct v4l2_requestbuffers req;
    int count;
    int i;
    struct mmap_buffer *b;

    memset(&req, 0, sizeof(req));

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(c->fd, VIDIOC_REQBUFS, &req) == -1)
    {
        if (errno == EINVAL)
            fprintf(stderr, "Error: does not support memory mapping\n");
        else
            fprintf(stderr, "Error %s(%d) requesting memmap\n", strerror(errno), errno);
        return 0;
    }
    count = req.count;

    if (count < 2)
    {
        fprintf(stderr, "Insufficient buffer memory\n");
        return 0;
    }

    c->buffers = (struct mmap_buffer *) calloc(count, sizeof(*c->buffers));
    if (!c->buffers)
    {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }

    for (i = 0, b = c->buffers; i < count; i++, b++)
    {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if (xioctl(c->fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            fprintf(stderr, "Error: query buf %d returns %s(%d)\n", i, strerror(errno), errno);
            return 0;
        }

        b->length = buf.length;
        b->start =
            mmap(NULL /* start anywhere */,
                  buf.length,
                  PROT_READ | PROT_WRITE /* required */,
                  MAP_SHARED /* recommended */,
                  c->fd, buf.m.offset);
        if (b->start == MAP_FAILED)
        {
            fprintf(stderr, "Error %s(%d): could not map buffer %d\n", strerror(errno), errno, i);
            return 0;
        }
    }

    return count;
}

static void uninit_device(capture_t *c)
{
    int i;
    for (i = 0; i < c->buffer_count; i++)
        munmap(c->buffers[i].start, c->buffers[i].length);
    free(c->buffers);
    c->buffer_count = 0;
}

static int start_capturing(capture_t *c)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < c->buffer_count; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(c->fd, VIDIOC_QBUF, &buf) == -1)
        {
            fprintf(stderr, "Error %s(%d) querying the buffer\n", strerror(errno), errno);
            return -1;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(c->fd, VIDIOC_STREAMON, &type) == -1)
    {
        fprintf(stderr, "Error %s(%d) streaming on\n", strerror(errno), errno);
        return -1;
    }

    return 0;
}

static void stop_capturing(capture_t *c)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(c->fd, VIDIOC_STREAMOFF, &type);
}




int capture_start(capture_t *c, const char *name, int width, int height, int fps)
{
    c->width = width;
    c->height = height;

    c->fd = open_device(name);
    if (c->fd < 0)
        return c->fd;

    if (init_device(c, width, height, fps) < 0)
        return -1;

    c->buffer_count = init_mmap(c);
    if (c->buffer_count <= 0)
        return -1;

    if (start_capturing(c))
        return -1;

    return 0;
}

void capture_stop(capture_t *c)
{
    stop_capturing(c);
    uninit_device(c);
    close(c->fd);
}

int capture_grab(capture_t *c)
{
    struct v4l2_buffer buf;
    struct mmap_buffer *b;

    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(c->fd, VIDIOC_DQBUF, &buf) == -1)
    {
        if (errno == EAGAIN)
            return 0;
        fprintf(stderr, "Error %s(%d): while getting buffer\n", strerror(errno), errno);
        return -1;
    }

    assert(buf.index < c->buffer_count);

    b = c->buffers + buf.index;
    c->last_frame_ptr = b->start;
    c->last_frame_len = buf.bytesused;

    if (xioctl(c->fd, VIDIOC_QBUF, &buf) == -1)
    {
        fprintf(stderr, "Error %s(%d): while setting buffer %d back up\n", strerror(errno), errno, buf.index);
        return -1;
    }

    return 1;
}

int capture_clear(capture_t *c1, capture_t *c2, int threshold)
{
    int count = 0;
    int discard = 0;
    while (count < threshold)
    {
        if ((c1 && capture_grab(c1) != 0) || (c2 && capture_grab(c2) != 0))
        {
            discard++;
            count = 0;
        }
        else
            count++;
    }
    return discard;
}


void * capture_retrieve(capture_t *c, int bytes, filter_t *filter)
{
    void *data = NULL;
    int s;

    s = vision_snapshot_number();
    if (s >= 0)
    {
        FILE *fp;
        fp = fopen(vision_file_template(s, "yuv", "raw"), "w");
        if (fp)
        {
            fwrite(c->last_frame_ptr, 1, c->width * c->height * 2, fp);
            fclose(fp);
        }
    }


    data = calloc(1, c->width * c->height * bytes);

    if (bytes == 3)
    {
        yuyv_to_rgb24 (c->last_frame_ptr, data, c->width, c->height, filter);
    }
    else if (bytes == 1)
    {
        yuyv_to_8(c->last_frame_ptr, data, c->width, c->height, filter);
    }
    else
    {
        free(data);
        data = NULL;
    }

    return data;
}

int capture_query_control(capture_t *c, int id, struct v4l2_queryctrl *ctrl)
{
    ctrl->id = id;
    return xioctl(c->fd, VIDIOC_QUERYCTRL, ctrl);
}

int capture_set_control(capture_t *c, int id, int val)
{
    struct v4l2_control ctrl;
    ctrl.id = id;
    ctrl.value = val;
    return xioctl(c->fd, VIDIOC_S_CTRL, &ctrl);
}

int capture_get_control(capture_t *c, int id, int *val)
{
    int rc;
    struct v4l2_control ctrl;
    ctrl.id = id;
    rc = xioctl(c->fd, VIDIOC_G_CTRL, &ctrl);
    if (rc == 0)
        *val = ctrl.value;
    return rc;
}

int capture_yuv_to_rgb(void *in, void *out, int width, int height, int colors, filter_t *filter)
{
    if (colors == 3)
    {
        yuyv_to_rgb24 (in, out, width, height, filter);
        return 0;
    }
    else if (colors == 1)
    {
        yuyv_to_8(in, out, width, height, filter);
        return 0;
    }
    return -1;
}
