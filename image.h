#if defined(__cplusplus)
extern "C" {
#endif
void process_blur(IplImage *img, char *type, struct timeval *t);
void perform_canny(IplImage *img, struct timeval *t, double threshold, int display);
void find_contours(IplImage *img, struct timeval *t, int display, int level);
#if defined(__cplusplus)
};
#endif

