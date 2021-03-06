#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include "fitsio.h"

long width=0;
long height=0;

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
			

			fits_read_img(fptr,TBYTE,1,nelements,NULL,*image, &anynul, &status);
			fits_close_file(fptr, &status);
			return status;
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

int main(int argc, char* argv[]) {
	unsigned char* a;
	unsigned char* b;
	unsigned char* c;
	long nelements;
	int i;
	int errors = 0;
	unsigned char maxi = 0;
	unsigned char delta;
	
	if (argc != 4){
		printf("Usage: %s <raw> <offset or dark> <result>\n",argv[0]);
		return -1;
	}
	
	if (read_fits(argv[1],&a) == 0){
		if (read_fits(argv[2],&b) == 0){	
			nelements = width*height;
			c = malloc(sizeof(unsigned char)*nelements);
			for(i=0;i<nelements;i++){
				if (a[i] >= b[i])
					c[i] = a[i] - b[i];
				else {
					c[i] = 0;
					delta = b[i] - a[i];
					if (delta > maxi) maxi = delta;
					errors++;
				}
			}
			printf("%d errors : maxi =%d\n",errors,maxi);
			
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
