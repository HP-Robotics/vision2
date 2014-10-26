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
    int min_u;
    int max_u;
    int min_v;
    int max_v;
    int min_y;
    int max_y;
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
void * capture_retrieve(capture_t *c, int bytes, filter_t *filter);
void capture_clear(capture_t *c1, capture_t *c2, int threshold);


#if defined(__cplusplus)
};
#endif

