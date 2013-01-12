/*
===============================================================================

  FILE:  precisioncheck.cpp
  
  CONTENTS:
   
    The program takes as input an indexed mesh and a number of precision bits
    and reports the maximal absolute error and the average error that will be
    introduced by quantization.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2001-2004  martin isenburg@cs.unc.edu
    
    This software is distributed for evaluation purposes only
    WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    12 March 2004 -- created
  
===============================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "micode.h"

void VecZero3dv(double v[3])
{
  v[0] = 0.0;
  v[1] = 0.0;
  v[2] = 0.0;
}

void VecSelfScalarDiv3dv(double v[3], double s)
{
  v[0] /= s;
  v[1] /= s;
  v[2] /= s;
}

void VecSelfAdd3dv(double v[3], double a[3])
{
  v[0] += a[0];
  v[1] += a[1];
  v[2] += a[2];
}

void VecAbsDiff3dv(double v[3], double a[3], double b[3])
{
  v[0] = a[0] - b[0];
  v[1] = a[1] - b[1];
  v[2] = a[2] - b[2];
	if (v[0] < 0.0) v[0] = -v[0];
	if (v[1] < 0.0) v[1] = -v[1];
	if (v[2] < 0.0) v[2] = -v[2];
}

void VecUpdateMax3dv(double max[3], double v[3])
{
  if (v[0]>max[0]) max[0]=v[0];
  if (v[1]>max[1]) max[1]=v[1];
  if (v[2]>max[2]) max[2]=v[2];
}

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"precisioncheck -i mesh.ply \n");
  fprintf(stderr,"precisioncheck -b 12 -i mesh.ply\n");
  fprintf(stderr,"precisioncheck -h\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  int i,j;
  int bits = 16;
  char* file_name_in = 0;

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name_in = argv[i];
    }
    else if (strcmp(argv[i],"-b") == 0)
    {
      i++;
      bits = atoi(argv[i]);
    }
    else
    {
      usage();
    }
  }

  if (file_name_in == 0)
  {
    usage();
  }

  miMesh* mimesh = loadMesh(file_name_in);

  if (mimesh == 0)
  {
    fprintf(stderr,"ERROR: failed to load mesh from %s\n", file_name_in);
    exit(1);
  }

  int nverts = mimesh->nverts;
  float* vertices = mimesh->vertices;
  float* qvertices = (float*)malloc(sizeof(float)*3*nverts);

  memcpy(qvertices, vertices, sizeof(float)*3*nverts);

  quantize(nverts, qvertices, bits);

  double vertex[3];
  double qvertex[3];
  double abs_error[3];

  double max_error[3];
  double avg_error[3];

  VecZero3dv(max_error);
  VecZero3dv(avg_error);

  for (i = 0; i < nverts; i++)
  {
    for (j = 0; j < 3; j++)
    {
      vertex[j] = vertices[3*i+j];
      qvertex[j] = qvertices[3*i+j];
    }
  	VecAbsDiff3dv(abs_error,vertex,qvertex);
  	VecUpdateMax3dv(max_error,abs_error);
  	VecSelfAdd3dv(avg_error,abs_error);
  }

  VecSelfScalarDiv3dv(avg_error,(double)nverts);

  printf("\n");
  printf("at %d bits for x, y, z\n", bits);
  
  for (j = 0; j < 3; j++)
  {
    printf("\n");
    printf("max error %1.30f\n",max_error[j]);
    printf("avg error %1.30f\n",avg_error[j]);
  }
  return 1;
}
