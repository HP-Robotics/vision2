/*-------------------------------------------------------------------------
* image.h
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
void process_blur(IplImage *img, char *type, struct timeval *t);
void perform_canny(IplImage *img, struct timeval *t, double threshold, int display);
void perform_sobel(IplImage *img, struct timeval *t, int display);
void find_contours(IplImage *img, struct timeval *t, int display, int level);
int Hough(IplImage *img, struct timeval *t, int display);
void print_real_average(char *buf, int buflen);
void perform_fast(IplImage *img, struct timeval *t, int display);
#if defined(__cplusplus)
};
#endif

