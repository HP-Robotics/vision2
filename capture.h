/*-------------------------------------------------------------------------
* capture.h - Sub routines to grab data from a v4l camera
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
#if defined(__cplusplus)
extern "C" {
#endif
struct mmap_buffer
{
    void   *start;
    size_t  length;
};

typedef struct color_filter
{
    int min_y;
    int max_y;
    int min_u;
    int max_u;
    int min_v;
    int max_v;

    /* If this is on, we apply all kinds of fancy logic
       to try to keep things green */
    int apply_fancy_logic;
} filter_t;

typedef struct capture_control
{
    int fd;
    struct mmap_buffer *buffers;
    int buffer_count;
    int width;
    int height;
    int fps;
    void *last_frame_ptr;
    int last_frame_len;
} capture_t;

int capture_start(capture_t *c, const char *name, int width, int height, int fps);
void capture_stop(capture_t *c);

int capture_grab(capture_t *c);
void * capture_retrieve(capture_t *c, int bytes, filter_t *filter, int raw);
int capture_clear(capture_t *c1, capture_t *c2, int threshold);

int capture_query_control(capture_t *c, int id, struct v4l2_queryctrl *ctrl);
int capture_set_control(capture_t *c, int id, int val);
int capture_get_control(capture_t *c, int id, int *val);
int capture_yuv_to_rgb(void *in, void *out, int width, int height, int colors, filter_t *filter);

#if defined(__cplusplus)
};
#endif

