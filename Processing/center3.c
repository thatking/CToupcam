#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include "fitsio.h"
#include <limits.h>
#include <math.h>

long width=0;
long height=0;

int xmini, ymini;

unsigned char* a;
unsigned char* b;
unsigned char* aa;
unsigned char* bb;

typedef struct {
	int n;
	int x;
	int y;
	int dx;
	int dy;
} Tpivot;

typedef struct {
	int x;
	int y;
} Px;


int read_fits(char* path, unsigned char** image)
{
	fitsfile *fptr;    
    int status = 0;
    unsigned int bitpix, naxis;
    long naxes[2];
    long nelements;
    int anynul;
	if (!fits_open_file(&fptr, path, READONLY, &status))
    {
		if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status) )
        {
			printf("bitpix=%d naxes=%d width=%ld height=%ld\n",bitpix,naxis,naxes[0],naxes[1]);
			nelements = naxes[0]*naxes[1];
			if (width==0 && height==0){
				width = naxes[0];
				height = naxes[1];
			} else {
				if (width != naxes[0] || height != naxes[1]){
					printf("naxes error: %ldx%ld instead of %ldx%ld\n",naxes[0],naxes[1],width,height);
					return -1;
				}
			}
			
			if (naxis != 2){
				printf("Process only 2D fits\n");
				fits_close_file(fptr, &status); 
				return -1;
			}

			if (bitpix != 8){
				printf("Process only 8 bit fits\n");
				fits_close_file(fptr, &status); 
				return -1;
			}
			
			*image = malloc(sizeof(unsigned char)*nelements);
			
			if (bitpix == 8){
				fits_read_img(fptr,TBYTE,1,nelements,NULL,*image, &anynul, &status);
				fits_close_file(fptr, &status);
				if (status != 0) printf("status = %d\n",status);
				return status;
			} 
			
		  }
     }
     return -1;		
}

int write_fits(char* path,unsigned char* image)
{
	fitsfile *fptrout;
	unsigned int naxis = 2;
    long naxes[2];
    int status = 0;
    long nelements = width*height;
    naxes[0] = width;
	naxes[1] = height;
	fits_create_file(&fptrout,path, &status);
	fits_create_img(fptrout, BYTE_IMG, naxis, naxes, &status);
	fits_write_img(fptrout, TBYTE, 1, nelements, image, &status);
	fits_close_file(fptrout, &status);
}

int get_max(int*a,int na,int* upper){
	int i,ii;
	int n,maxi;
	maxi = 0;
	ii = -1;
	for(i=0;i<na;i++){
		n = a[i];
		if ((maxi < n) && (n < *upper)){
			maxi = n;
			ii = i;
		}
	}
	*upper=maxi;
	return ii;	
}

int get_max_uchar(unsigned char*a,int na,int* upper){
	int i,ii;
	unsigned char n,maxi;
	maxi = 0;
	ii = -1;
	for(i=0;i<na;i++){
		n = a[i];
		if ((maxi < n) && (n < *upper)){
			maxi = n;
			ii = i;
		}
	}
	*upper=maxi;
	return ii;	
}

int get_index(int*a,int na,int value){
	int i;
	int n;
	for(i=0;i<na;i++){
		n = a[i];
		if(n == value) return i;
	}
	for(i=0;i<na;i++){
		n = a[i];
		if(n == value+1) return i;
		if(n == value-1) return i;
	}
	return -1;
}
			
	
		
int main(int argc, char* argv[]) {

	unsigned char* c;
	long nelements;

	unsigned char factor;
	unsigned char thresholda;
	unsigned char thresholdb;
	unsigned long  ul;
	
	unsigned char mini;
	unsigned char maxi;
	int i,j,k,l;
	int n;
	int na, nb;
	unsigned char us;
	int va,vb,vc,size,index;
	
	Tpivot pivota[4];
	Tpivot pivotb[4];
	
	Px* pxa;
	Px* pxb; 
	Px* pxl;
	
	int* a2;
	int* b2;
	
	int iter;
	
	int dx,dy,distance,distmini;
	
	int x,y;
	int x0, y0;
	int x1, y1;
	
	int upper;
	
	int lista[10];
	int listb[10];
	
	int pointa[20];
	int pointb[20];
	
	int point[20];
	
	int la,lb;
	
	int go;
	
	int nsample;
	
	int ii;
	
	
	nsample = 20;
	

	x=0;
	y=0;
	x0=0;
	y0=0;
	
	xmini = 0;
	ymini = 0;
	
	unsigned int* score;
	
	
	if (argc != 5){
		printf("Usage: %s <image1> <image2> <result> <factor>\n",argv[0]);
		return -1;
	}
	
	factor = atoi(argv[4]);
	
	if (read_fits(argv[1],&a) == 0){
		if (read_fits(argv[2],&b) == 0){
			nelements = height * width;
			
			// detect maxi/mini
			mini = UCHAR_MAX;
			maxi = 0;
			for (i=0;i<nelements;i++){
				us = a[i];
				if ( us < mini ) mini = us;
				if ( us > maxi ) maxi = us;
			}
			ul = (unsigned long)(maxi-mini)*(unsigned long)factor;
			thresholda = (unsigned char)(ul/100L)+mini;
			printf("mini=%d - maxi=%d - threshold=%d\n",mini,maxi,thresholda);
			// 
			// detect stars
			aa = malloc(sizeof(unsigned char)*nelements);
			for (i=0;i<nelements;i++){
				aa[i] = 0;
			}
			n = 0;
			for (y=1;y<height-1;y++)
				for(x=1;x<width-1;x++){
					i = y*width+x;
					us = a[i];
					aa[i] = us;
					for(y0=y-1;y0<=y+1;y0++){
						for(x0=x-1;x0<=x+1;x0++){
							if (a[y0*width+x0] > us)
							{
								aa[i] = 0;
								break;
							}
						}
					}
					if (aa[i]!=0){
						//printf("(%d,%d)\n",x,y);
						n++;
					}	
				}
								
			printf("detect %d/%ld\n",n,nelements);
			
			na = n;
			pxa = malloc(sizeof(Px)*na);
			
			// filter 
			upper = INT_MAX;
			j = 0;
			for (i=0;i<nsample;i++){
				//printf(". %d\n",upper);
				ii = get_max_uchar(aa,nelements,&upper);
				//printf(".. %d\n",upper);
				if (upper == 0) break;
				pxa[j].x = ii % width;
				pxa[j].y = ii / width;
				j++;
			}
				
			na = j;
						
			a2 = malloc(sizeof(int)*na*na);
			for(i=0;i<na;i++)
				for(j=0;j<na;j++){
					dx=pxa[i].x-pxa[j].x;
					dy=pxa[i].y-pxa[j].y;
					distance=dx*dx+dy*dy;
					distance = (int)sqrt((double)distance);
					//printf("%d:%d -> %d\n",i,j,distance);
					a2[j*na+i]=distance;	
				}
			
			// detect maxi/mini
			mini = UCHAR_MAX;
			maxi = 0;
			for (i=0;i<nelements;i++){
				us = b[i];
				if ( us < mini ) mini = us;
				if ( us > maxi ) maxi = us;
			}
			ul = (unsigned long)(maxi-mini)*(unsigned long)factor;
			thresholdb = (unsigned char)(ul/100L)+mini;
			printf("mini=%d - maxi=%d - threshold=%d\n",mini,maxi,thresholdb);	
			// detect stars
			bb = malloc(sizeof(unsigned char)*nelements);
			for (i=0;i<nelements;i++){
				bb[i] = 0;
			}
			n = 0;
			for (y=1;y<height-1;y++)
				for(x=1;x<width-1;x++){
					i = y*width+x;
					us = b[i];
					bb[i] = us;
					for(y0=y-1;y0<=y+1;y0++){
						for(x0=x-1;x0<=x+1;x0++){
							if (b[y0*width+x0] > us)
							{
								bb[i] = 0;
								break;
							}
						}
					}
					if (bb[i]!=0){
						//printf("(%d,%d)\n",x,y);
						n++;
					}		
				}
			printf("detect %d/%ld\n",n,nelements);	
			
			nb = n;
			pxb = malloc(sizeof(Px)*nb);
			
			// filter 
			upper = INT_MAX;
			j = 0;
			for (i=0;i<nsample;i++){
				ii = get_max_uchar(bb,nelements,&upper);
				//printf(".. %d\n",upper);
				if (upper == 0) break;
				pxb[j].x = ii % width;
				pxb[j].y = ii / width;
				j++;
			}
			
			nb = j;
					
			b2 = malloc(sizeof(int)*nb*nb);
			for(i=0;i<nb;i++)
				for(j=0;j<nb;j++){
					dx=pxb[i].x-pxb[j].x;
					dy=pxb[i].y-pxb[j].y;
					distance=dx*dx+dy*dy;
					distance = (int)sqrt((double)distance);
					//printf("%d:%d -> %d\n",i,j,distance);
					b2[j*nb+i]=distance;	
				}	
				
				
			// segment correspondance
			upper = INT_MAX;
			go = 1;
			i = 0;
			while ( go ) {
				la = get_max(a2,na*na,&upper);
				if (la>=0) {
					//printf("upper=%d a=%d\n",upper,la);
					lb = get_index(b2,nb*nb,upper);
					if (lb>=0) {
						//printf("found b=%d\n",lb);
						lista[i] = la;
						listb[i++] = lb;
						if (i==10) go=0;
					}
				} else {
					go = 0;
				}
			}
			
			size = i;
			printf("distance match=%d\n",size);
			
			if (size == 0) {
				printf ("No matching found!\n");
				exit(0);
			}
			
			for(j=0;j<size;j++){
				la = lista[j];
				pointa[2*j  ]=la%na;
				pointa[2*j+1]=la/na;
				lb = listb[j];
				pointb[2*j  ]=lb%nb;
				pointb[2*j+1]=lb/nb;
			}
			
			pxl = malloc(sizeof(Px)*size);
			
			// middle of segments
			
			for(j=0;j<size;j++){
				x0 = pointa[2*j];
				x1 = pointa[2*j+1];
				y0 = pointb[2*j];
				y1 = pointb[2*j+1];
				//printf("a (%d,%d) - (%d,%d)\n",pxa[x0].x,pxa[x0].y,pxa[x1].x,pxa[x1].y);
				x = (pxa[x0].x+pxa[x1].x) - (pxb[y0].x+pxb[y1].x);
				x = x/2;
				//printf("b (%d,%d) - (%d,%d)\n",pxb[y0].x,pxb[y0].y,pxb[y1].x,pxb[y1].y);
				y = (pxa[x0].y+pxa[x1].y) - (pxb[y0].y+pxb[y1].y);
				y = y/2;
				pxl[j].x = x;
				pxl[j].y = y;
				//printf("dx=%d , dy=%d\n",x,y);
			}
			
			score = malloc(sizeof(unsigned int)*size);
			
			for (i=0;i<size;i++){
				score[i] = 0;
				for(j=i;j<size;j++)
					if ((pxl[i].x == pxl[j].x) && (pxl[i].y == pxl[j].y))
						score[i]++;
			}
			
			maxi = 0;
			index = 0;
			for (i=0;i<size;i++){
				//printf("%d:%d (%d,%d)\n",i,score[i],pxl[i].x,pxl[i].y);
				if (score[i] > maxi){
					maxi = score[i];
					index = i;
				}
			}
				
			printf("max score= %d index=%d\n",maxi,index);
			
			if (size == 1) {
				index = 0;
			} else {
				if (maxi < 2) {
					printf("No enough matching\n");
					free(pxl);
					free(score);
					exit(0);
				}
			}
			
			x = (pxl[index].x);
			y = (pxl[index].y);

			
			free(pxl);
			free(score);
			
			xmini = -x;
			ymini = -y;
			
			
			printf("translate: x:%d y:%d\n",xmini,ymini);
	   
		   // output fits is the image2 translated ( relative to image1)
		   c = malloc(sizeof(unsigned char)*nelements);
		   for(y=0;y< height;y++)
			for(x=0;x< width; x++){
				x0=x+xmini;
				y0=y+ymini;
				if ( ( x0 >= width ) || ( x0 < 0) ||  ( y0 >= height ) || (y0 < 0) ){
						c[x+y*width] = 0;
				} else {
						c[x+y*width] = b[x0+y0*width];
				}
			}

		   remove(argv[3]);
		   if (write_fits(argv[3],c) == 0){
				free(a); free(b); free(c);
				return 0;
			}
		   
	   }
		   
	}
					
	free(a); free(b); free(c);
	return -1;
	
}
