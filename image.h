#if defined(__cplusplus)
extern "C" {
#endif
void process_blur(IplImage *img, char *type, struct timeval *t, int display);
void find_contours(IplImage *img, struct timeval *t, int display);
#if defined(__cplusplus)
};
#endif

