/*-------------------------------------------------------------------------
* image.cpp - These are functions that require C++
*             (Some OpenCV functions are C++ only)
*
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
#include <cstring>
#include <stdio.h>
#include <sys/time.h>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "opencv2/features2d/features2d.hpp"

#include "image.h"
#include "vision.h"

using namespace cv;

void process_blur(IplImage *img, char *type, struct timeval *t)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);
    Mat a = cvarrToMat(img);
    //GaussianBlur(a, a, Size(3, 3), 1.0);
    blur(a, a, Size(5, 5),Point(-1,1),BORDER_DEFAULT);
    //medianBlur(a,a, 5);
    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);
}

void perform_canny(IplImage *img, struct timeval *t, double threshold, int display)
{
    struct timeval start, end, diff;
    int s;
    gettimeofday(&start, NULL);

    Mat a = cvarrToMat(img);
    Canny(a, a, threshold, threshold * 3.0, 3);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    if (display)
        cvShowImage("Canny", img);

    s = vision_snapshot_number();
    if (s >= 0)
        cvSaveImage(vision_file_template(s, "canny", "png"), img, 0);
}
void perform_sobel(IplImage *img, struct timeval *t, int display)
{
    struct timeval start, end, diff;
    int s;
    gettimeofday(&start, NULL);

    Mat a = cvarrToMat(img);
    Sobel(a, a, -1,1, 0, 5, 2, 0, BORDER_DEFAULT);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    if (display)
        cvShowImage("Sobel", img);

    s = vision_snapshot_number();
    if (s >= 0)
        cvSaveImage(vision_file_template(s, "sobel", "png"), img, 0);
}
RNG rng(12345);
void find_contours(IplImage *img, struct timeval *t, int display, int level)
{
    struct timeval start, end, diff;
    int s;
    gettimeofday(&start, NULL);

    Mat copy = Mat(img, true);

    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;

    findContours(copy, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

    

    s = vision_snapshot_number();

    if (display || s >= 0)
    {
        Mat cnt_img = Mat::zeros(copy.size(), CV_8UC3);
        for (size_t i = 0; i < contours.size(); i++)
        {
            Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
            drawContours( cnt_img, contours, i, color, 2, 8, hierarchy, 0, Point() );
        }
        if (display)
            imshow("Contours", cnt_img);

        //if (s >= 0)
        //    cvSaveImage(vision_file_template(s, "canny", "png"), cnt_img, 0);
    }

}
void Hough(IplImage *img, struct timeval *t, int display){

	struct timeval start, end, diff;
    gettimeofday(&start, NULL);
    int s;
    Mat copy=Mat(img,true);
    vector<Vec4i> lines;
    
    HoughLinesP(copy, lines, 1, CV_PI/180, 15, 25, 20 );
    
    gettimeofday(&end,NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);
    
    s=vision_snapshot_number();
    
    if(display || s>=0){
    	Mat cnt_img = Mat::zeros(copy.size(), CV_8UC3);
    	int size=lines.size();
    	for( size_t i = 0; i < lines.size(); i++ )
    	{
        	Vec4i l = lines[i];
        	line( cnt_img, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0,0,255), 1,4);
    	}
    	int testline=0;
    	int matches=0;
    	while (testline<size){
    		Vec2i a1=Vec2i(lines[testline][0],lines[testline][1]);
    		Vec2i a2=Vec2i(lines[testline][2],lines[testline][3]);
    		int loopline=0;
    		while(loopline<size){
    				float theta=0;
    				Vec2i b1=Vec2i(lines[loopline][0],lines[loopline][1]);
    				Vec2i b2=Vec2i(lines[loopline][2],lines[loopline][3]);
    				theta=abs(float((a2-a1).dot(b2-b1)));
    				//printf("%g, %g\n",theta,0*(norm(a2-a1)*norm(b2-b1)));
    				if(theta>0.95*(norm(a2-a1)*norm(b2-b1)) && loopline!=testline){
    				
    					float a2Distance=pow(norm(a2-b1),2)-pow(((a2-b1).dot(b2-b1)/norm(b2-b1)),2);
    					//printf("%g, %g, %g, %g, %g\n",float((a2-b1).dot(b2-b1)),(a2-b1).dot(b2-b1)/norm(b2-b1),norm(b2-b1),norm(a2-b1),a2Distance);
    					float a1Distance=pow(norm(a1-b1),2)-pow(((a1-b1).dot(b2-b1)/norm(b2-b1)),2);
    					//printf("%g, %g\n",a2Distance,a1Distance);
    					if(a2Distance<50*50 && a1Distance<50*50){
    						matches+=1;
    						Vec2i Bleep1Vector=float((b2-b1).dot(a1-b1))/norm(b2-b1)/norm(b2-b1)*(b2-b1);
    						Vec2i Bleep2Vector=float((b2-b1).dot(a2-b1))/norm(b2-b1)/norm(b2-b1)*(b2-b1);
    						
    						Vec2i print1point=(a1-b1)-Bleep1Vector;
    						Vec2i print2point=(a2-b1)-Bleep2Vector;
    						//printpoint=printpoint*int(((b2-b1).dot(a1-b1))/norm(b2-b1)/norm(b2-b1));
    						//printf("%g, %g, %g, %g\n",norm(print1point),norm(print2point),a1Distance,a2Distance);
    						Vec2i merge1=(a1-b1)-print1point/2;
    						//Point2f(((a1-b1)-a1Distance*(b2-b1)/norm(b2-b1))/2);
    						Vec2i merge2=(a2-b1)-print2point/2;
    						//line(cnt_img, Point(merge1),Point(merge2),Scalar(255,255,255),1,4);
    						merge1=merge1+b1;
    						merge2=merge2+b1;
    						//line(cnt_img, Point(merge1),Point(merge2),Scalar(0,255,255),1,4);    						
    						float maxdistance = (a2-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						float mindistance = maxdistance;
    						
    						float testdistance = (a1-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						printf("%f, %f, %f\n",testdistance,maxdistance,mindistance);
    						if(testdistance<mindistance){
    							mindistance=testdistance;
    						}
    						if(testdistance>maxdistance){
    							maxdistance=testdistance;
    						}
    						testdistance = (b1-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						printf("%f, %f, %f\n",testdistance,maxdistance,mindistance);
    						if(testdistance<mindistance){
    							mindistance=testdistance;
    						}
    						if(testdistance>maxdistance){
    							maxdistance=testdistance;
    						}
    						testdistance = (b2-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						printf("%f, %f, %f\n",testdistance,maxdistance,mindistance);
    						if(testdistance<mindistance){
    							mindistance=testdistance;
    						}
    						if(testdistance>maxdistance){
    							maxdistance=testdistance;
    						}
    						printf("%f, %f, %f\n",testdistance,maxdistance,mindistance);
    						Vec2i temppoint=(maxdistance/norm(merge2-merge1))*(merge2-merge1)+merge1;
    						merge1=(mindistance/norm(merge2-merge1))*(merge2-merge1)+merge1;
    						merge2=temppoint;
    						
    						line(cnt_img, Point(merge1),Point(merge2),Scalar(0,255,0),1,4);
    						
    						//Point2f(((a2-b1)-a2Distance*(b2-b1)/norm(b2-b1))/2);
    						printf("%g vs. %g\n",theta,0.9*norm(a2-a1)*(norm(b2-b1)));
    						//printf("%f versus %f\n",merge2height,merge1height);
    						//line(cnt_img, a1,b1,Scalar(255,0,0),1,4);
    						//line(cnt_img, a2,b2,Scalar(0,255,0),1,4);
    						
    						lines[testline][0]=merge1[0];
    						lines[testline][1]=merge1[1];
    						lines[testline][2]=merge2[0];
    						lines[testline][3]=merge2[1];
    						
    						lines[loopline]=lines[size];
    						size--;

    					}
    					else{
    						loopline++;
    					}
    				}
    				else{
    					loopline++;
    				}
    		}
    		testline++;
    	}
    	testline=0;
    	while(testline<size){
    		Vec4i out=lines[testline];
    		line(cnt_img,Point(out[0],out[1]),Point(out[2],out[3]),Scalar(255,255,0),1,4);
    		testline++;
    	}
    	printf("%d Lines found, %d Matches found: \n", size,matches);
    	if(display){
    		imshow("HoughLines", cnt_img);
    	}
    	if (s>=0){
    		IplImage temp = cnt_img;
    	    cvSaveImage(vision_file_template(s, "Hough", "png"), &temp, 0);
    	    }
		}
		gettimeofday(&end, NULL);
    	timersub(&end, &start, &diff);
    	timeradd(t, &diff, t);
}
void perform_fast(IplImage *img, struct timeval *t, int display)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    FastFeatureDetector detector(10, true);

    Mat a = cvarrToMat(img);
    vector<KeyPoint> keypoints;
    detector.detect(a, keypoints);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    timeradd(t, &diff, t);

    if (display)
        drawKeypoints(a, keypoints, a, Scalar::all(-1), DrawMatchesFlags::DRAW_OVER_OUTIMG|DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
}
