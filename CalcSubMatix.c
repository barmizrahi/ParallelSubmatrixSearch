#include <string.h>
#include <stdlib.h>
#include <omp.h>
#include "CalcSubMatix.h"

int main(int argc, char *argv[]) {
	int my_rank; /* rank of process */
	int numberOfProcesses; /* number of processes */

	MPI_Status status; /* return status for receive */
	ObjectToSearch **allObjectsToSearch;

	int numberOfImages = 0, numberOfObjectsToSearch = 0, init;
	double matchingValue, start, end;

	/* start up MPI */

	MPI_Init(&argc, &argv);

	/* find out process rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* find out number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);
	
	if (my_rank != 0) {
		//all proc must recv all the object from file from proc 0
		int flagRecvAllObj;
		MPI_Bcast(&init, 1, MPI_INT, 0, MPI_COMM_WORLD);
		if (init == 0) { //zero proc finish read from file do loop until he get 1 from zero 
			MPI_Recv(&init, 1, MPI_INT, 0, 5, MPI_COMM_WORLD, &status); //block until zero free him

		}
		//free from init then recv all from zero
		flagRecvAllObj = recvAllObjectsFromZero(&allObjectsToSearch,
				&numberOfObjectsToSearch);
		if (!flagRecvAllObj) {
			printf(
					"An Error occurred While Recv or Malooc Therefor The Program Can't Work");
			return 0;
		}
		MPI_Bcast(&matchingValue, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); //Bcast of matchingValue from zero procc
		processeNotZero(allObjectsToSearch, &numberOfObjectsToSearch,
				&matchingValue);

	} else {
		Image **allImages;
		char *fileName = "input.txt";
		init = 0;
		MPI_Bcast(&init, 1, MPI_INT, 0, MPI_COMM_WORLD);
		readFromFile(&allObjectsToSearch, &allImages, &matchingValue,
				&numberOfImages, &numberOfObjectsToSearch, fileName);

		start = MPI_Wtime();
		init = 1;
#pragma omp parallel for		
		for (int i = 1; i < numberOfProcesses; i++) {
			MPI_Send(&init, 1, MPI_INT, i, 5, MPI_COMM_WORLD);
		}
		sendAllObjects(allObjectsToSearch, &numberOfObjectsToSearch);
		MPI_Bcast(&matchingValue, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		processeZero(&allImages, &numberOfImages, &numberOfProcesses);
		//when finish the loop free the images
		freeAllImage(&allImages, &numberOfImages);
		end = MPI_Wtime();
		printf("time %.2f \n", (end - start));

	}
	/* shut down MPI */
	freeAllObjectToSearch(&allObjectsToSearch, &numberOfObjectsToSearch);

	MPI_Finalize();

	return 0;
}

int processeZero(Image ***allImages, int *numberOfImages,
		int *numberOfProcesses) {
	int
			imageToSendIndex = 0 /*to track which image to sand from all the images*/,
			flagFoundObjInImg = 0,
			procssesRank/*update all the time when recv*/, flagStopWork = 0;
	//int picToPrintIndex, objToPrintIndex, indexX, indexY;
	FILE *fp;
	char *fileName = "output.txt";
	if ((fp = fopen(fileName, "w")) == 0) {
		printf("cannot open file %s for writing\n", fileName);
		return -1;
	}
	MPI_Status stat, stat2;
	int numberOfProcSearching = *numberOfProcesses - 1; //all the processes that searching allObjects
	int imagesToSeach = *numberOfImages; //number of images that need to search them

//if there are other processes that still working on searching OR there are any images to search them
	while (numberOfProcSearching < (*numberOfProcesses - 1) || imagesToSeach > 0) {

		// Wait for any message from any of the processes that running
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
		procssesRank = stat.MPI_SOURCE; //rank of recv process
		// now we split the work in accordance of recv tag
		if (stat.MPI_TAG == TAG_FREE_TO_WORK) {
			//if enter, the slave_rank ask for image to work with
			MPI_Recv(&flagFoundObjInImg, 1, MPI_INT, procssesRank,
			TAG_FREE_TO_WORK, MPI_COMM_WORLD, &stat);
			//if there are Images to search
			if (imagesToSeach > 0) {
				sendApic(allImages, &imageToSendIndex, &procssesRank); //send a image to procssesRank
				imageToSendIndex = imageToSendIndex + 1; // promote in 1
				imagesToSeach = imagesToSeach - 1; // we need to lower imagesToSeach by 1
				numberOfProcSearching = numberOfProcSearching - 1; //we need to lower numberOfProcSearching by 1 becuse now we give to proc work and we have 1 less than numberOfProcSearching

			} else {
				// if enter here its means that the procc wants another image to work but we dont have any image left
				flagStopWork = 1;
				//then send to procssesRank to stop working
				MPI_Send(&flagStopWork, 1, MPI_INT, procssesRank,
						TAG_FINISH_WORK,
						MPI_COMM_WORLD);
			}
		} else {
			//if enter here its means that the procssesRank send us a result from his searching
			numberOfProcSearching = numberOfProcSearching + 1; //procssesRank finished his search and now we have 1 more free procc
			//enter to flagFoundObjInImg the result
			handleResult(fp, procssesRank);
			if (imagesToSeach == 0) { //if there are no left images then stop working otherwise keep
				flagStopWork = 1;
				MPI_Send(&flagStopWork, 1, MPI_INT, procssesRank,
						TAG_FINISH_WORK,
						MPI_COMM_WORLD);
			}
		}
	}
	return 1;
}
void handleResult(FILE *fp, int procssesRank) {
	int flagFoundObjInImg, picToPrintIndex, objToPrintIndex, indexX, indexY;
	MPI_Status stat;
	MPI_Recv(&flagFoundObjInImg, 1, MPI_INT, procssesRank, TAG_FOUND,
	MPI_COMM_WORLD, &stat);

	if (flagFoundObjInImg == 1) {
		//if enter then the procssesRank flagFoundObjInImg any object in the image he has and now recv all the data from him
		MPI_Recv(&picToPrintIndex, 1, MPI_INT, procssesRank, TAG_FOUND,
		MPI_COMM_WORLD, &stat);
		MPI_Recv(&objToPrintIndex, 1, MPI_INT, procssesRank, TAG_FOUND,
		MPI_COMM_WORLD, &stat);
		MPI_Recv(&indexX, 1, MPI_INT, procssesRank, TAG_FOUND,
		MPI_COMM_WORLD, &stat);
		MPI_Recv(&indexY, 1, MPI_INT, procssesRank, TAG_FOUND,
		MPI_COMM_WORLD, &stat);
		//printf("Picture %d found object %d in position (%d,%d)\n",
			//	picToPrintIndex, objToPrintIndex, indexX, indexY);
		fprintf(fp, "Picture %d found object %d in position (%d,%d)\n",
				picToPrintIndex, objToPrintIndex, indexX, indexY);

	} else {

		MPI_Recv(&picToPrintIndex, 1, MPI_INT, procssesRank, TAG_FOUND,
		MPI_COMM_WORLD, &stat);
		printf("No Objects Found In Picture %d\n", picToPrintIndex);
		fprintf(fp, "No Objects Found In Picture %d\n", picToPrintIndex);
	}
}
void processeNotZero(ObjectToSearch **allObjectToSearch,
		int *numberOfObjectToSearch, double *matchingValue) {
	MPI_Status stat, stat2;
	Image *imageToSearch = NULL;
	int stopped = 0;
	int match = 0;
	do {
		//send msg to zero that the procc is free and adk for image
		MPI_Send(&match, 1, MPI_INT, 0, TAG_FREE_TO_WORK, MPI_COMM_WORLD); //recv something, the tag is the matter
		MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &stat); //block until recv TAG from 0
		if (stat.MPI_TAG == TAG_NEW_IMAGE) {
			//if enter then we have new image to search!
			handleNewImage(imageToSearch, allObjectToSearch, matchingValue,
					numberOfObjectToSearch, stat);
		} else {
			//if here then zero send us tag that diffrent from TAG_JOB_DATA then we need to stop working
			MPI_Recv(&stopped, 1, MPI_INT, 0, TAG_FINISH_WORK, MPI_COMM_WORLD,
					&stat2);
			stopped = 1;
		}

	} while (stopped == 0);
}

void handleNewImage(Image *imageToSearch, ObjectToSearch **allObjectToSearch,
		double *matchingValue, int *numberOfObjectToSearch, MPI_Status stat) {

	int match = 0, flagGetImage;
	int imageoPrintIndex, objToPrintIndex = -1, indexX = 0, indexY = 0,
			indexXToPrint = 0, indexYToPrint = 0; //data to send to zero if match
	flagGetImage = getPicFromMaster(&imageToSearch, stat); //get the new image from zero
	if (flagGetImage != 1) {
		return;
	}
	imageoPrintIndex = imageToSearch->id;
	int flag = 1;
	omp_set_num_threads(4);
#pragma omp parallel for shared(flag, objToPrintIndex, match, indexYToPrint, indexXToPrint)
	for (int i = 0; i < *numberOfObjectToSearch; i++) {
		if (flag) {
			match = searchObjInImg(imageToSearch, allObjectToSearch[i],
					*matchingValue, &indexX, &indexY);
			//printf("match = %d x = %d y = %d flag = %d image = %d objToPrintIndex = %d\n" ,match,indexX, indexY ,flag,imageToSearch->id,objToPrintIndex);
			if (match == 1 && flag) {
				objToPrintIndex = i; //save the location of i
				indexXToPrint = indexX;
				indexYToPrint = indexY;
				flag = 0;
			}
		}
	}
	if (objToPrintIndex >= 0 && objToPrintIndex < *numberOfObjectToSearch) {
		match = 1;
	} else {
		match = 0;
		MPI_Send(&match, 1, MPI_INT, 0, TAG_FOUND, MPI_COMM_WORLD);
		MPI_Send(&imageoPrintIndex, 1, MPI_INT, 0, TAG_FOUND, MPI_COMM_WORLD);
		return;
	}
	// send result to zero match or not and the tag_found
	MPI_Send(&match, 1, MPI_INT, 0, TAG_FOUND, MPI_COMM_WORLD);
	if (match == 1) {
		//if enter that we got a match and need to send to zero all the data he wait for
		objToPrintIndex = allObjectToSearch[objToPrintIndex]->id;
		//send all the data to zero
		MPI_Send(&imageoPrintIndex, 1, MPI_INT, 0, TAG_FOUND,
		MPI_COMM_WORLD);
		MPI_Send(&objToPrintIndex, 1, MPI_INT, 0, TAG_FOUND,
		MPI_COMM_WORLD);
		MPI_Send(&indexXToPrint, 1, MPI_INT, 0, TAG_FOUND, MPI_COMM_WORLD);
		MPI_Send(&indexYToPrint, 1, MPI_INT, 0, TAG_FOUND, MPI_COMM_WORLD);

	}

}

int searchObjInImg(Image *image, ObjectToSearch *ObjectToSearch,
		double matchingValue, int *posX, int *posY) {

	if ((image->dim) < (ObjectToSearch->dim)) { //if the object bigger then the image then return
		return -1;
	}
	int loopCounter = (image->dim) - (ObjectToSearch->dim) + 1 /*, flag*/;
	float totDiff = 0;
	int oDim = ObjectToSearch->dim;
	int **object = ObjectToSearch->obj;
	int **img = image->img;
	int iRow, iCol, oRow, oCol , flag = 1;
	for (iRow = 0; iRow < loopCounter; iRow++) { //runs over all possible rows in pic
		for (iCol = 0; iCol < loopCounter; iCol++) { ////runs over all possible cols in pic
			totDiff = 0;
			for (oRow = iRow; oRow < iRow + (oDim); oRow++) { //runs over all possible rows in object
				for (oCol = iCol; oCol < iCol + (oDim); oCol++) { //runs over all possible cols in object
					totDiff += diff(img[oRow][oCol],
							object[oRow - iRow][oCol - iCol]);
					if (totDiff > matchingValue) { //check if we already bigger then matchingValue if yes then we can stop searching in those index i j n o
						break;
					}
					
				}
			}

			if (matchingValue > totDiff) {
				*posX = iRow;
				*posY = iCol;
				return 1;
			}
		}

	}

	//we done seraching all image indexes and not return 1 then we know the object is not in this image
	return -1;
}
float diff(int imageNumber, int objectNumber) {
	//return abs(p-o)/p
	return abs(imageNumber - objectNumber) / (float) imageNumber;
}
void sendApic(Image ***allPics, int *pos, int *sender) {
//sends the picture in index to sender
	MPI_Send(&(allPics[0][*pos]->id), 1, MPI_INT, *sender, TAG_NEW_IMAGE,
	MPI_COMM_WORLD);
	MPI_Send(&(allPics[0][*pos]->dim), 1, MPI_INT, *sender, TAG_NEW_IMAGE,
	MPI_COMM_WORLD);
	for (int j = 0; j < (allPics[0][*pos]->dim); j++) {
		MPI_Send(allPics[0][*pos]->img[j], (allPics[0][*pos]->dim), MPI_INT,
				*sender, TAG_NEW_IMAGE, MPI_COMM_WORLD);
	}
}

int getPicFromMaster(Image **imageToSearch, MPI_Status status) {
	imageToSearch[0] = (Image*) malloc(sizeof(Image)); //new image
	if (!imageToSearch[0]) {
		printf("malloc failed imageToSearch\n");
		return -1;
	}
	MPI_Recv(&imageToSearch[0]->id, 1, MPI_INT, 0, TAG_NEW_IMAGE,
	MPI_COMM_WORLD, &status); //recv id of image

	MPI_Recv(&imageToSearch[0]->dim, 1, MPI_INT, 0, TAG_NEW_IMAGE,
	MPI_COMM_WORLD, &status); //recv dim of image

	imageToSearch[0]->img = (int**) malloc(
			sizeof(int*) * (imageToSearch[0]->dim)); // new mat of int
	if (!imageToSearch[0]->img) {
		printf("malloc failed imageToSearch[0]\n");
		return -1;
	}

	for (int j = 0; j < (imageToSearch[0]->dim); j++) {
		imageToSearch[0]->img[j] = (int*) malloc(
				sizeof(int) * (imageToSearch[0]->dim));
		if (!imageToSearch[0]->img[j]) {
			printf("malloc failed imageToSearch[0]->img[j]\n");
			return -1;
		}
		MPI_Recv(imageToSearch[0]->img[j], (imageToSearch[0]->dim), MPI_INT, 0,
		TAG_NEW_IMAGE,
		MPI_COMM_WORLD, &status); //recv the whole line imageToSearch[0]->img[j]
	}
	return 1;
}

int sendAllObjects(ObjectToSearch **allObjectsToSearch,
		int *numberOfallObjectsToSearch) {
	MPI_Bcast(numberOfallObjectsToSearch, 1, MPI_INT, 0, MPI_COMM_WORLD); //bcast the number of object

	for (int i = 0; i < *numberOfallObjectsToSearch; i++) {
		MPI_Bcast(&(allObjectsToSearch[i]->id), 1, MPI_INT, 0, MPI_COMM_WORLD); //bcast the id
		MPI_Bcast(&(allObjectsToSearch[i]->dim), 1, MPI_INT, 0, MPI_COMM_WORLD); //bcast the dim
		for (int j = 0; j < (allObjectsToSearch[i]->dim); j++) {
			MPI_Bcast(allObjectsToSearch[i]->obj[j],
					(allObjectsToSearch[i]->dim), MPI_INT, 0,
					MPI_COMM_WORLD); //bcast the whole row allObjectsToSearch[i]->obj[j]
		}
	}
	return 1;
}

int recvAllObjectsFromZero(ObjectToSearch ***allObjectsToSearch,
		int *numberOfObjectsToSearch) {
	MPI_Bcast(numberOfObjectsToSearch, 1, MPI_INT, 0, MPI_COMM_WORLD); //recv number of object
	allObjectsToSearch[0] = (ObjectToSearch**) malloc(
			sizeof(ObjectToSearch*) * (*numberOfObjectsToSearch));
	if (!allObjectsToSearch) {
		printf("malloc failed allObjectsToSearch[0]\n");
		return -1;
	}
	for (int i = 0; i < *numberOfObjectsToSearch; i++) { //for each object
		allObjectsToSearch[0][i] = (ObjectToSearch*) malloc(
				sizeof(ObjectToSearch)); //malloc a  new ObjectToSearch
		if (!allObjectsToSearch[0][i]) {
			printf("malloc failed allObjectsToSearch[0][i]\n");
			return -1;
		}
		MPI_Bcast(&(allObjectsToSearch[0][i]->id), 1, MPI_INT, 0,
		MPI_COMM_WORLD); // get the id of allObjectsToSearch[0][i]
		MPI_Bcast(&(allObjectsToSearch[0][i]->dim), 1, MPI_INT, 0,
		MPI_COMM_WORLD); // // get the dim of allObjectsToSearch[0][i]

		allObjectsToSearch[0][i]->obj = (int**) malloc(
				sizeof(int*) * allObjectsToSearch[0][i]->dim); //malloc new array of int
		if (!allObjectsToSearch[0][i]->obj) {
			printf("malloc failed allObjectsToSearch[0][i]->obj");
			return -1;
		}
		
		for (int j = 0; j < allObjectsToSearch[0][i]->dim; j++) { //for each row malloc
			allObjectsToSearch[0][i]->obj[j] = (int*) malloc(
					(sizeof(int) * (allObjectsToSearch[0][i]->dim)));
			if (!allObjectsToSearch[0][i]->obj[j]) {
				printf("malloc failed allObjectsToSearch[0][i]->obj[j]\n");
				return -1;
			}
		}

		for (int j = 0; j < (allObjectsToSearch[0][i]->dim); j++) { //recv the whole row from zero into allObjectsToSearch[0][i]->obj[j]
			MPI_Bcast(allObjectsToSearch[0][i]->obj[j],
					(allObjectsToSearch[0][i]->dim),
					MPI_INT, 0, MPI_COMM_WORLD);
		}
	}
	return 1;
}

int readFromFile(ObjectToSearch ***allObjectsToSearch, Image ***allImages,
		double *matchingValue, int *numberOfImages,
		int *numberOfObjectsToSearch, char *fileName) {
	FILE *fp;
	if ((fp = fopen(fileName, "r")) == 0) {
		printf("cannot open file %s for reading\n", fileName);
		return -1;
	}

	fscanf(fp, "%lf", matchingValue);//matching value

	fscanf(fp, "%d", numberOfImages);//number of pictures
	allImages[0] = (Image**) malloc(sizeof(Image*) * (*numberOfImages));
	if (!allImages[0]) {
		printf("error malloc readFromFile all images\n");
		return -1;
	}
	for (int i = 0; i < (*numberOfImages); i++) {
		allImages[0][i] = (Image*) malloc(sizeof(Image));
		if (!allImages[0][i]) {
			printf("error malloc readFromFile all allImages[0][i]\n");
			return -1;
		}

		fscanf(fp, "%d", &allImages[0][i]->id);//read id

		fscanf(fp, "%d", &allImages[0][i]->dim);//read dim

		allImages[0][i]->img = (int**) malloc(
				sizeof(int*) * allImages[0][i]->dim);
		if (!allImages[0][i]->img) {
			printf("error malloc readFromFile allImages[0][i]->pic\n");
			return -1;
		}
		for (int k = 0; k < allImages[0][i]->dim; k++) {
			allImages[0][i]->img[k] = (int*) malloc(
					sizeof(int) * allImages[0][i]->dim);
			if (!allImages[0][i]->img[k]) {
				printf("error malloc readFromFile allImages[0][i]->pic[k]\n");
				return -1;
			}
			for (int t = 0; t < allImages[0][i]->dim; t++) {
				fscanf(fp, "%d", &allImages[0][i]->img[k][t]);
			}
		}
	}
		//read objectsToSearch
		//number of objectsToSearch
	fscanf(fp, "%d", numberOfObjectsToSearch);
	allObjectsToSearch[0] = (ObjectToSearch**) malloc(
			sizeof(ObjectToSearch*) * (*numberOfObjectsToSearch));
	if (!allObjectsToSearch[0]) {
		printf("error malloc readFromFile allObjectsToSearch[0]\n");
		return -1;
	}
	for (int i = 0; i < (*numberOfObjectsToSearch); i++) {
		allObjectsToSearch[0][i] = (ObjectToSearch*) malloc(
				sizeof(ObjectToSearch));
		if (!allObjectsToSearch[0][i]) {
			printf("error malloc readFromFile allObjectsToSearch[0][i]\n");
			return -1;
		}
		//read id
		fscanf(fp, "%d", &(allObjectsToSearch[0][i]->id));
		//read dim
		fscanf(fp, "%d", &(allObjectsToSearch[0][i]->dim));
		//read matrix
		allObjectsToSearch[0][i]->obj = (int**) malloc(
				sizeof(int*) * (allObjectsToSearch[0][i]->dim));
		if (!allObjectsToSearch[0][i]->obj) {
			printf("error malloc readFromFile allObjectsToSearch[0][i]->obj\n");
			return -1;
		}
		for (int j = 0; j < (allObjectsToSearch[0][i]->dim); j++) {
			allObjectsToSearch[0][i]->obj[j] = (int*) malloc(
					sizeof(int) * (allObjectsToSearch[0][i]->dim));
			if (!allObjectsToSearch[0][i]->obj[j]) {
				printf(
						"error malloc readFromFile allObjectsToSearch[0][i]->obj[k]\n");
				return -1;
			}
			for (int n = 0; n < (allObjectsToSearch[0][i]->dim); n++) {
				fscanf(fp, "%d", &allObjectsToSearch[0][i]->obj[j][n]);
			}
		}
	}
	fclose(fp);
	return 1;
}

void freeAllObjectToSearch(ObjectToSearch ***allObjectsToSearch,
		int *numberOfObjectsToSerach) {
	if (allObjectsToSearch[0] != NULL) {
		for (int i = 0; i < (*numberOfObjectsToSerach); i++) {
			if (allObjectsToSearch[0][i] != NULL) {
			omp_set_num_threads(2);
			#pragma omp parallel for
				for (int j = 0; j < allObjectsToSearch[0][i]->dim; j++) {
					free(allObjectsToSearch[0][i]->obj[j]);
				}
			}
			free(allObjectsToSearch[0][i]->obj);
			free(allObjectsToSearch[0][i]);
		}
		free(allObjectsToSearch[0]);
	}
}

void freeAllImage(Image ***allImages, int *numberOfImages) {
	if (allImages[0] != NULL) {
		for (int i = 0; i < (*numberOfImages); i++) {
			freeImg(&allImages[0][i]);
			free(allImages[0][i]->img);
			free(allImages[0][i]);
		}
		free(allImages[0]);
	}
}

void freeImg(Image **image) {
	if (image[0] != NULL) {
	omp_set_num_threads(2);
	#pragma omp parallel for
		for (int j = 0; j < image[0]->dim; j++) {
			free(image[0]->img[j]);
		}
	}
}
