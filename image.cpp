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
vector<Vec4i> CullLines(vector<Vec4i> lines, float angle,int distance, int gap){
		int size=lines.size();
    	int testline=0;
    	int matches=0;
    	while (testline<size){

    		int loopline=0;
    		while(loopline<size){
    				Vec2i a1=Vec2i(lines[testline][0],lines[testline][1]);
    				Vec2i a2=Vec2i(lines[testline][2],lines[testline][3]);
    				size=lines.size();
    				float theta=0;
    				Vec2i b1=Vec2i(lines[loopline][0],lines[loopline][1]);
    				Vec2i b2=Vec2i(lines[loopline][2],lines[loopline][3]);
    				theta=abs(float((a2-a1).dot(b2-b1)));
    				if(theta>angle*(norm(a2-a1)*norm(b2-b1)) && loopline!=testline){
    				
    					float a2Distance=pow(norm(a2-b1),2)-pow(((a2-b1).dot(b2-b1)/norm(b2-b1)),2);
    					float a1Distance=pow(norm(a1-b1),2)-pow(((a1-b1).dot(b2-b1)/norm(b2-b1)),2);
    					if(a2Distance<distance*distance && a1Distance<distance*distance){
    						
    						matches+=1;
    						Vec2i Bleep1Vector=float((b2-b1).dot(a1-b1))/norm(b2-b1)/norm(b2-b1)*(b2-b1);
    						Vec2i Bleep2Vector=float((b2-b1).dot(a2-b1))/norm(b2-b1)/norm(b2-b1)*(b2-b1);
    						
    						Vec2i print1point=(a1-b1)-Bleep1Vector;
    						Vec2i print2point=(a2-b1)-Bleep2Vector;
    						Vec2i merge1=(a1-b1)-print1point/2;
    						Vec2i merge2=(a2-b1)-print2point/2;
    						merge1=merge1+b1;
    						merge2=merge2+b1;    						
    						float Distances[4];
    						Distances[0]=(a2-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						Distances[1]=(a1-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						Distances[2]=(b1-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						Distances[3]=(b2-merge1).dot(merge2-merge1)/norm(merge2-merge1);
    						
    						if((Distances[0]<max(Distances[2],Distances[3])+gap && Distances[0]>min(Distances[2],Distances[3])-gap)
    						||(Distances[1]<max(Distances[2],Distances[3])+gap && Distances[1]>min(Distances[2],Distances[3])-gap)){
    						float maxdistance = Distances[0];
    						
    						float mindistance = maxdistance;
    						//printf("%f, %f, %f\n",Distances[0],maxdistance,mindistance);
    						for(int i =1;i<4;i++){
    							//printf("%f, %f, %f\n",Distances[i],maxdistance,mindistance);
    							if(maxdistance<Distances[i]){
    								maxdistance=Distances[i];
    							}
    							if(mindistance>Distances[i]){
    								mindistance=Distances[i];
    							}
    						}
    						
    						//printf("%f, %f, %f\n",maxdistance,mindistance);
    						Vec2i temppoint=(maxdistance/norm(merge2-merge1))*(merge2-merge1)+merge1;
    						merge1=(mindistance/norm(merge2-merge1))*(merge2-merge1)+merge1;
    						merge2=temppoint;
    						//printf("\n");
    						Vec4i Out=Vec4i(merge1[0],merge1[1],merge2[0],merge2[1]);
    						lines[testline]=Out;
    						printf("Replacing line #%d. Deleting line #%d\n",testline,loopline);
    						lines.erase(lines.begin()+loopline);
    						size=lines.size();
    						loopline--;
    						if(loopline<testline){
    							testline--;
    						}
    						}

    					}
    				}
    				loopline++;
    		}
    		testline++;
    	}
    	return lines;

}
vector<Vec4i> GetGoals(vector<Vec4i> lines, int error){
	vector<Vec4i> Out;
	unsigned int lineIndex=0;
	while(lineIndex<lines.size()){
		Vec4i Main=lines[lineIndex];
		int intersects=0;
		unsigned int loopIndex=0;
		while(loopIndex<lines.size() && intersects<2){
			Vec4i Comp=lines[loopIndex];
			
			Vec2i a1=Vec2i(Main[0],Main[1]);
			Vec2i a2=Vec2i(Main[2],Main[3]);
			Vec2i b1=Vec2i(Comp[0],Comp[1]);
			Vec2i b2=Vec2i(Comp[2],Comp[3]);
			
			Vec2i t1=(a2-a1)*error/norm(a2-a1)+a2;
			Vec2i t2=a1-(a2-a1)*error/norm(a2-a1);
			Vec2i c1=(b2-b1)*error/norm(b2-b1)+b2;
			Vec2i c2=b1-(b2-b1)*error/norm(b2-b1);
			
			
			Vec4i Merge=Vec4i(t1[0],t1[1],t2[0],t2[1]);
			if(float((t2-t1)[0]*(c1-t1)[1]-(t2-t1)[1]*(c1-t1)[0])/((t2-t1)[0]*(c2-t1)[1]-(t2-t1)[1]*(c2-t1)[0])<0){
				if(float((c2-c1)[0]*(t1-c1)[1]-(c2-c1)[1]*(t1-c1)[0])/((c2-c1)[0]*(t2-c1)[1]-(c2-c1)[1]*(t2-c1)[0])<0){
					intersects++;
					//Out.push_back(Merge);
				}
			}
			loopIndex++;
		}
		if(intersects<2){
			lineIndex++;
		}
		else{
			Out.push_back(Main);
			lines.erase(lineIndex+lines.begin());
		}
	}
	int changes=1;
	vector<Vec4i> temp;
	while(changes>0){
		changes=0;
		lineIndex=0;
		while(lineIndex<Out.size()){
			int intersects=0;
			Vec4i Main=Out[lineIndex];
			unsigned int loopIndex=0;
			while(loopIndex<lines.size()){
				Vec4i Comp=lines[loopIndex];
			
				Vec2i a1=Vec2i(Main[0],Main[1]);
				Vec2i a2=Vec2i(Main[2],Main[3]);
				Vec2i b1=Vec2i(Comp[0],Comp[1]);
				Vec2i b2=Vec2i(Comp[2],Comp[3]);
			
				Vec2i t1=(a2-a1)*error/norm(a2-a1)+a2;
				Vec2i t2=a1-(a2-a1)*error/norm(a2-a1);
				Vec2i c1=(b2-b1)*error/norm(b2-b1)+b2;
				Vec2i c2=b1-(b2-b1)*error/norm(b2-b1);
			
			
				Vec4i Merge=Vec4i(t1[0],t1[1],t2[0],t2[1]);
				if(float((t2-t1)[0]*(c1-t1)[1]-(t2-t1)[1]*(c1-t1)[0])/((t2-t1)[0]*(c2-t1)[1]-(t2-t1)[1]*(c2-t1)[0])<0 && 
				float((c2-c1)[0]*(t1-c1)[1]-(c2-c1)[1]*(t1-c1)[0])/((c2-c1)[0]*(t2-c1)[1]-(c2-c1)[1]*(t2-c1)[0])<0){
					intersects++;
					changes++;
					Out.push_back(Comp);
					lines.erase(loopIndex+lines.begin());
					
					//Out.push_back(Merge);
					
				}
				else{
					loopIndex++;
				}
			}
			if(intersects<1){
				lineIndex++;
			}
			else{
				temp.push_back(Main);
				Out.erase(lineIndex+Out.begin());
			}
		}
		
	
	}
	for(int i=0;i<temp.size();i++){
		Out.push_back(temp[i]);
	}
	return Out;
}
vector<Vec4i> CullNonGoals(vector<Vec4i> lines, Vec2i p, float anglethreshold, int error){
	vector<Vec4i> Out;
	unsigned int lineIndex=0;
	while(lineIndex<lines.size()){
		Vec4i Main=lines[lineIndex];
		Vec2i a1=Vec2i(Main[0],Main[1]);
		Vec2i a2=Vec2i(Main[2],Main[3]);
		if(abs(float((a2-a1).dot(p)))>anglethreshold*norm(a2-a1)*norm(p)){
			Out.push_back(Main);
		}
		
		lineIndex++;
	}
	vector<Vec4i> temp;
	lineIndex=0;
	while(lineIndex<Out.size()){
		Vec4i Main=Out[lineIndex];
		unsigned int loopIndex=0;
		bool exit=0;
		while(loopIndex<lines.size() && !exit){
			Vec4i Comp=lines[loopIndex];
			
			Vec2i a1=Vec2i(Main[0],Main[1]);
			Vec2i a2=Vec2i(Main[2],Main[3]);
			Vec2i b1=Vec2i(Comp[0],Comp[1]);
			Vec2i b2=Vec2i(Comp[2],Comp[3]);
			
			Vec2i t1=(a2-a1)*error/norm(a2-a1)+a2;
			Vec2i t2=a1-(a2-a1)*error/norm(a2-a1);
			Vec2i c1=(b2-b1)*error/norm(b2-b1)+b2;
			Vec2i c2=b1-(b2-b1)*error/norm(b2-b1);
			
			
			Vec4i Merge=Vec4i(t1[0],t1[1],t2[0],t2[1]);
			if(float((t2-t1)[0]*(c1-t1)[1]-(t2-t1)[1]*(c1-t1)[0])/((t2-t1)[0]*(c2-t1)[1]-(t2-t1)[1]*(c2-t1)[0])<0 && 
			float((c2-c1)[0]*(t1-c1)[1]-(c2-c1)[1]*(t1-c1)[0])/((c2-c1)[0]*(t2-c1)[1]-(c2-c1)[1]*(t2-c1)[0])<0){
				unsigned int line2Index=lineIndex+1;
				while(line2Index<Out.size() && !exit){
					Vec4i Main2=Out[line2Index];
					
					Vec2i d1=Vec2i(Main2[0],Main2[1]);
					Vec2i d2=Vec2i(Main2[2],Main2[3]);
					Vec2i m1=(d2-d1)*error/norm(d2-d1)+d2;
					Vec2i m2=d1-(d2-d1)*error/norm(d2-d1);
					if(float((m2-m1)[0]*(c1-m1)[1]-(m2-m1)[1]*(c1-m1)[0])/((m2-m1)[0]*(c2-m1)[1]-(m2-m1)[1]*(c2-m1)[0])<0 && 
					float((c2-c1)[0]*(m1-c1)[1]-(c2-c1)[1]*(m1-c1)[0])/((c2-c1)[0]*(m2-c1)[1]-(c2-c1)[1]*(m2-c1)[0])<0){
						temp.push_back(Comp);
						lines.erase(loopIndex+lines.begin());
						exit=1;
					}
					
					
					line2Index++;
				}
				
					//Out.push_back(Merge);
					
			}
			loopIndex++;
		}
		lineIndex++;
	
	}
	for(int i=0;i<temp.size();i++){
		Out.push_back(temp[i]);
	}
	
	return Out;
}
vector<vector<Vec4i> > FindGoals(vector<Vec4i> lines, Vec2i p, float anglethreshold, int error){
	vector<vector<Vec4i> > Out;
	vector<Vec4i> sides;
	unsigned int lineIndex=0;
	while(lineIndex<lines.size()){
		Vec4i Main=lines[lineIndex];
		Vec2i a1=Vec2i(Main[0],Main[1]);
		Vec2i a2=Vec2i(Main[2],Main[3]);
		if(abs(float((a2-a1).dot(p)))>anglethreshold*norm(a2-a1)*norm(p)){
			sides.push_back(Main);
		}
		
		lineIndex++;
	}
	vector<Vec4i> temp;
	lineIndex=0;
	while(lineIndex<sides.size()){
		Vec4i Main=sides[lineIndex];
		unsigned int loopIndex=0;
		bool exit=0;
		while(loopIndex<lines.size()){
			Vec4i Comp=lines[loopIndex];
			
			Vec2i a1=Vec2i(Main[0],Main[1]);
			Vec2i a2=Vec2i(Main[2],Main[3]);
			Vec2i b1=Vec2i(Comp[0],Comp[1]);
			Vec2i b2=Vec2i(Comp[2],Comp[3]);
			
			Vec2i t1=(a2-a1)*error/norm(a2-a1)+a2;
			Vec2i t2=a1-(a2-a1)*error/norm(a2-a1);
			Vec2i c1=(b2-b1)*error/norm(b2-b1)+b2;
			Vec2i c2=b1-(b2-b1)*error/norm(b2-b1);
			
			
			if(float((t2-t1)[0]*(c1-t1)[1]-(t2-t1)[1]*(c1-t1)[0])/((t2-t1)[0]*(c2-t1)[1]-(t2-t1)[1]*(c2-t1)[0])<0 && 
			float((c2-c1)[0]*(t1-c1)[1]-(c2-c1)[1]*(t1-c1)[0])/((c2-c1)[0]*(t2-c1)[1]-(c2-c1)[1]*(t2-c1)[0])<0){
				unsigned int line2Index=lineIndex+1;
				while(line2Index<sides.size()){
					Vec4i Main2=sides[line2Index];
					
					Vec2i d1=Vec2i(Main2[0],Main2[1]);
					Vec2i d2=Vec2i(Main2[2],Main2[3]);
					Vec2i m1=(d2-d1)*error/norm(d2-d1)+d2;
					Vec2i m2=d1-(d2-d1)*error/norm(d2-d1);
					if(float((m2-m1)[0]*(c1-m1)[1]-(m2-m1)[1]*(c1-m1)[0])/((m2-m1)[0]*(c2-m1)[1]-(m2-m1)[1]*(c2-m1)[0])<0 && 
					float((c2-c1)[0]*(m1-c1)[1]-(c2-c1)[1]*(m1-c1)[0])/((c2-c1)[0]*(m2-c1)[1]-(c2-c1)[1]*(m2-c1)[0])<0){
						temp.push_back(Main);
						temp.push_back(Comp);
						temp.push_back(Main2);
						Out.push_back(temp);
						temp.clear();
						//lines.erase(loopIndex+lines.begin());
						exit=1;
					}
					
					
					line2Index++;
				}
				
					//Out.push_back(Merge);
					
			}
			loopIndex++;
		}
		lineIndex++;
	
	}
	/*
	for(int i=0;i<temp.size();i++){
		Out.push_back(temp[i]);
	}
	*/
	printf("%d\n",Out.size());
	return Out;
}
vector<vector<Vec4i> >FixUpGoals(vector<vector<Vec4i> > goals, float margin){
	unsigned int i=0;
	vector<vector<Vec4i> > Out;
	while(i<goals.size()){
		//printf("hi\n");
	
		vector<Vec4i> testgoal=goals[i];
		Vec4i side1=testgoal[0];
		Vec4i base=testgoal[1];
		Vec4i side2=testgoal[2];
		
		Vec2i s1p1=Vec2i(side1[0],side1[1]);
		Vec2i s1p2=Vec2i(side1[2],side1[3]);
		Vec2i s2p1=Vec2i(side2[0],side2[1]);
		Vec2i s2p2=Vec2i(side2[2],side2[3]);
		
		Vec2i b1p1=Vec2i(base[0],base[1]);
		Vec2i b1p2=Vec2i(base[2],base[3]);
		bool edit[2]={0,0};		
		Vec2i test1=(b1p1-s1p1)-(s1p2-s1p1)*(b1p1-s1p1).dot(s1p2-s1p1)/norm(s1p2-s1p1)/norm(s1p2-s1p1);
		Vec2i test2=(b1p2-s1p1)-(s1p2-s1p1)*(b1p2-s1p1).dot(s1p2-s1p1)/norm(s1p2-s1p1)/norm(s1p2-s1p1);

		if(norm(test1)>norm(test2)){
			if(norm(test2)<margin*norm(b1p2-b1p1) && norm(test1)<norm(b1p2-b1p1)){
				printf("Goal!\n");
				s1p1+=test2;
				s1p2+=test2;
			}
			else{
				printf("No Goal1\n");
				edit[0]=1;
			}
		}
		else{	
			if(norm(test1)<margin*norm(b1p2-b1p1) && norm(test2)<norm(b1p2-b1p1)){
				printf("Other Goal!\n");
				s1p1+=test1;
				s1p2+=test1;
			}
			else{
				printf("No Goal2\n");
				edit[0]=1;
			}
		}
		Vec2i btest1=(b1p1-s2p1)-(s2p2-s2p1)*(b1p1-s2p1).dot(s2p2-s2p1)/norm(s2p2-s2p1)/norm(s2p2-s2p1);
		Vec2i btest2=(b1p2-s2p1)-(s2p2-s2p1)*(b1p2-s2p1).dot(s2p2-s2p1)/norm(s2p2-s2p1)/norm(s2p2-s2p1);

		if(norm(btest1)>norm(btest2)){
			if(norm(btest2)<margin*norm(b1p2-b1p1)&& norm(btest1)<norm(b1p2-b1p1)){
				printf("2Goal!\n");
				s2p1+=btest2;
				s2p2+=btest2;
			}
			else{
				printf("2No Goal1\n");
				edit[1]=1;
			}
		}
		else{	
			if(norm(btest1) < margin*norm(b1p2-b1p1)&& norm(btest2)<norm(b1p2-b1p1)){
				printf("2Other Goal!\n");
				s2p1+=btest1;
				s2p2+=btest1;
			}
			else{
				printf("2No Goal2\n");
				edit[1]=1;
			}
		}
		/*
		Vec2f temp=-1*((s1p1-b1p1)-(s1p1-b1p1).dot(b1p2-b1p1)/(norm(b1p2-b1p1)*norm(b1p2-b1p1))*(b1p2-b1p1));
		Vec2i i1=s1p1-(s1p1-s1p2)*norm(temp)/(float((b1p1-b1p2)[1]*(s1p1-s1p2)[0]-(b1p1-b1p2)[0]*(s1p1-s1p2)[1])/(norm(b1p1-b1p2)*norm(s1p1-s1p2)))/norm(s1p1-s1p2);
		
		Vec2f temp2=(s2p1-b1p1)-(s2p1-b1p1).dot(b1p2-b1p1)/(norm(b1p2-b1p1)*norm(b1p2-b1p1))*(b1p2-b1p1);
		Vec2i i2=s2p1-(s2p1-s2p2)*norm(temp2)/((float((b1p1-b1p2)[1]*(s2p1-s2p2)[0]-(b1p1-b1p2)[0]*(s2p1-s2p2)[1]))/(norm(b1p1-b1p2)*norm(s2p1-s2p2)))/norm(s2p1-s2p2);
		*/
		float i1x=((s1p1[0]*s1p2[1]-s1p1[1]*s1p2[0])*(b1p1[0]-b1p2[0])-(s1p1[0]-s1p2[0])*(b1p1[0]*b1p2[1]-b1p1[1]*b1p2[0]))/((s1p1[0]-s1p2[0])*(b1p1[1]-b1p2[1])-(s1p1[1]-s1p2[1])*(b1p1[0]-b1p2[0]));
		float i1y=((s1p1[0]*s1p2[1]-s1p1[1]*s1p2[0])*(b1p1[1]-b1p2[1])-(s1p1[1]-s1p2[1])*(b1p1[0]*b1p2[1]-b1p1[1]*b1p2[0]))/((s1p1[0]-s1p2[0])*(b1p1[1]-b1p2[1])-(s1p1[1]-s1p2[1])*(b1p1[0]-b1p2[0]));
		Vec2i i1=Vec2i(i1x,i1y);
		float i2x=((s2p1[0]*s2p2[1]-s2p1[1]*s2p2[0])*(b1p1[0]-b1p2[0])-(s2p1[0]-s2p2[0])*(b1p1[0]*b1p2[1]-b1p1[1]*b1p2[0]))/((s2p1[0]-s2p2[0])*(b1p1[1]-b1p2[1])-(s2p1[1]-s2p2[1])*(b1p1[0]-b1p2[0]));
		float i2y=((s2p1[0]*s2p2[1]-s2p1[1]*s2p2[0])*(b1p1[1]-b1p2[1])-(s2p1[1]-s2p2[1])*(b1p1[0]*b1p2[1]-b1p1[1]*b1p2[0]))/((s2p1[0]-s2p2[0])*(b1p1[1]-b1p2[1])-(s2p1[1]-s2p2[1])*(b1p1[0]-b1p2[0]));
		Vec2i i2=Vec2i(i2x,i2y);
		//printf("%d %d %d %d\n",i1[0],i1[1],i2[0],i2[1]);
		//printf("%f,%f\n",temp2[0],temp2[1]);
		i++;
		
		if(norm(i1-s1p1)<=norm(i1-s1p2)){
			if(norm(i1-s1p1)<=margin*norm(s1p1-s1p2)){
				if(norm(i1-s1p2)<=norm(s1p1-s1p2)){
					i1=s1p1;
				}
				else{
					s1p1=i1;
				}
			}
			else{
				s1p1=i1;
			}
		}
		else{
			if(norm(i1-s1p2)<=margin*norm(s1p1-s1p2)){
				if(norm(i1-s1p1)<=norm(s1p1-s1p2)){
					i1=s1p2;
				}
				else{
					s1p2=i1;
				}
			}
			else{
				s1p2=i1;
			}
		}
		if(norm(i2-s2p1)<=norm(i2-s2p2)){
			if(norm(i2-s2p1)<=margin*norm(s2p1-s2p2)){
				if(norm(i2-s2p2)<=norm(s2p1-s2p2)){
					i2=s2p1;
				}
				else{
					s2p1=i2;
				}
			}
			else{
				s2p1=i2;
			}
		}
		else{
			if(norm(i2-s2p2)<=margin*norm(s2p1-s2p2)){
				if(norm(i2-s2p1)<=norm(s2p1-s2p2)){
					i2=s2p2;
				}
				else{
					s2p2=i2;
				}
			}
			else{
				s2p2=i2;
			}
		}
		
		vector<Vec4i> out;
		out.push_back(Vec4i(s1p1[0],s1p1[1],s1p2[0],s1p2[1]));
		out.push_back(Vec4i(i1[0],i1[1],i2[0],i2[1]));
		out.push_back(Vec4i(s2p1[0],s2p1[1],s2p2[0],s2p2[1]));
		Out.push_back(out);
		out.clear();
	}
	return Out;
}

void process_blur(IplImage *img, char *type, struct timeval *t)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);
    Mat a = cvarrToMat(img);
    GaussianBlur(a, a, Size(3, 3), 1.0);
    //blur(a, a, Size(5, 5),Point(-1,1),BORDER_DEFAULT);
    //medianBlur(a,a, 3);
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
    
    HoughLinesP(copy, lines, 1, CV_PI/180, 15, 10, 20 );
    
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
    	printf("# of lines:%d\n",size);
    	int distance=15;
    	float angle=.95;
    	int gap=10;
    	//while(lines.size()>=15){
    		lines=CullLines(lines,angle,distance,10);
    		//lines=CullLines(lines,angle,distance,10);
    		printf("end lines:%d\n",lines.size());
    		for(int i=0;i<lines.size();i++){
    			//line(cnt_img,Point(lines[i][0],lines[i][1]),Point(lines[i][2],lines[i][3]),Scalar(0,255,0),3,4);
    		}
    		
    		lines=CullNonGoals(lines,Vec2i(1,0),.95,gap);
    		for(int i=0;i<lines.size();i++){
    			line(cnt_img,Point(lines[i][0],lines[i][1]),Point(lines[i][2],lines[i][3]),Scalar(255,255,0),1,4);
    		}
    		lines=GetGoals(lines,gap);
    		distance++;
    		angle-=.01;
    		//gap--;
    	//}
    	vector<vector<Vec4i> > goals = FindGoals(lines,Vec2i(1,0),.95,gap);
    	printf("%d\n",goals.size());
    	
    	int testline=0;
    	size=lines.size();
    	
    	while(testline<size){
    		Vec4i out=lines[testline];
    		//line(cnt_img,Point(out[0],out[1]),Point(out[2],out[3]),Scalar(255,255,0),1,4);
    		testline++;
    	}
    	/*
    	for(unsigned int i=0;i<goals.size();i++){
    		vector<Vec4i> onegoal=goals[i];
    		for(unsigned int j=0;j<onegoal.size();j++){
    			Vec4i draw=onegoal[j];
    			//line(cnt_img,Point(draw[0],draw[1]),Point(draw[2],draw[3]),Scalar(0,255,(255/goals.size())*i),1,4);
    		}
    	}
    	*/
    	goals = FixUpGoals(goals,.2);
    	for(unsigned int i=0;i<goals.size();i++){
    		vector<Vec4i> onegoal=goals[i];
    		for(unsigned int j=0;j<onegoal.size();j++){
    			Vec4i draw=onegoal[j];
    			//printf("%d,%d,%d,%d\n",draw[0],draw[1],draw[2],draw[3]);
    			line(cnt_img,Point(draw[0],draw[1]),Point(draw[2],draw[3]),Scalar(255,255,255),1,4);
    		}
    	}
    	if(display){
    							imshow("HoughLines", cnt_img);
    							cnt_img=Mat::zeros(copy.size(), CV_8UC3);
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
