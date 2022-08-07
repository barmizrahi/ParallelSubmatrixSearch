#ifndef CALCSUBMATRIX_H_
#define CALCSUBMATRIX_H_
#include <math.h>
#include <stdio.h>
#include "mpi.h"

#define TAG_FREE_TO_WORK 0 //all procc but 0 send this , zero recv this
#define TAG_FOUND 1//all procc but 0 send this , zero recv this
#define TAG_FINISH_WORK 2//zero send to all procc to stop working
#define TAG_NEW_IMAGE 3//zero send to all procc that they now recv new image to work

typedef struct {
	int id;
	int dim;
	int **img;
} Image;

typedef struct {
	int id;
	int dim;
	int **obj;
} ObjectToSearch;

int readFromFile(ObjectToSearch ***allObjectToSearch, Image ***allImages,
		double *matchingValue, int *numberOfPictures,
		int *numberOfObjectToSearch, char *fileName);
int sendAllObjects(ObjectToSearch **allObjects, int *numberOfObjectToSearch);
int recvAllObjectsFromZero(ObjectToSearch ***allObjectsToSearch,
		int *numberOfObjectsToSearch);
int getPicFromMaster(Image **retPic, MPI_Status status);
int searchObjInImg(Image *image, ObjectToSearch *ObjectToSearch,
		double matchingValue, int *posX, int *posY);
int processeZero(Image ***allImages, int *numberOfImages,
		int *numberOfProcesses);
float diff(int imageNumber, int objectNumber);
void sendApic(Image ***allImages, int *pos, int *sender);
void freeAllObjectToSearch(ObjectToSearch ***allObjects,
		int *numberOfObjectToSearch);
void freeImg(Image **image);
void handleNewImage(Image *imageToSearch, ObjectToSearch **allObjectToSearch,
		double *matchingValue, int *numberOfObjectToSearch, MPI_Status stat);
void freeAllImage(Image ***allImage, int *numberOfImages);
void processeNotZero(ObjectToSearch **ObjectToSearch,
		int *numberOfObjectToSearch, double *matchingValue);
void handleResult(FILE *fp, int procssesRank);

#endif
