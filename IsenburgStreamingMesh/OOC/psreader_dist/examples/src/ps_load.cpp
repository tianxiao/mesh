/*
===============================================================================

  FILE:  ps_load.cpp
  
  CONTENTS:
  
    A demo program that uses the Processing Sequence API. Its main purpose
    is to provide other programmers an easy-to-follow example of how the
    Processing Sequence API can be used for some mesh processing task.

    This simple program loads the Processing Sequence in form of a standard
    indexed mesh into memory.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    11 September 2003 -- created initial version just after midnight 
  
===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psreader_oocc.h"
#include "psconverter.h"
#include "smreader_sma.h"

#include "vec3fv.h"
#include "vec3iv.h"

float* allocVertices(float* vertices, int number)
{
  if (vertices)
  {
    vertices = (float*)realloc(vertices, sizeof(float)*number*3);
  }
  else
  {
    vertices = (float*)malloc(sizeof(float)*number*3);
  }
  if (vertices == 0)
  {
    printf("ERROR: malloc for %d vertices failed.\nprobably the mesh is too big for in-core storage.\n", number);
    exit(-1);
  }
  return vertices;
}

void deallocVertices(float* vertices)
{
  free(vertices);
}

int* allocFaces(int* faces, int number)
{
  if (faces)
  {
    faces = (int*)realloc(faces, sizeof(int)*number*3);
  }
  else
  {
    faces = (int*)malloc(sizeof(int)*number*3);
  }
  if (faces == 0)
  {
    printf("ERROR: malloc for %d faces failed.\nprobably the mesh is too big for in-core storage.\n", number);
    exit(-1);
  }
  return faces;
}

void deallocFaces(int* faces)
{
  free(faces);
}

int main(int argc, char *argv[])
{
  char* file_name;

  if (argc == 2)
  {
    file_name = argv[1];
  }
  else
  {
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"ps_load <file_name>\n");
    exit(1);
  }

  PSreader* psreader = 0;

  if (strstr(file_name, "compressed"))
  {
    PSreader_oocc* psreader_oocc = new PSreader_oocc();
    psreader_oocc->open(file_name);
    psreader = psreader_oocc;
  }
  else if (strstr(file_name, ".sma") || strstr(file_name, ".obj") || strstr(file_name, ".smf"))
  {
    FILE* sma_fp = fopen(file_name, "r");
    if (sma_fp == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      exit(1);
    }
    SMreader_sma* smreader_sma = new SMreader_sma();
    smreader_sma->open(sma_fp);
    PSconverter* psconverter = new PSconverter();
    psconverter->open(smreader_sma, 256, 512);
    psreader = psconverter;
  }
  else
  {
    fprintf(stderr,"ERROR: cannot guess input type from file name\n");
    exit(1);
  }

  float* vertices = 0;
  int vertices_alloced = 1024;
  int vertices_number = 0;

  int* faces = 0;
  int faces_alloced = 512;
  int faces_number = 0;

  if (psreader->nverts == -1)
  {
    fprintf(stderr,"nverts not defined\n");
  }
  else
  {
    fprintf(stderr,"nverts %d\n",psreader->nverts);
    vertices_alloced = psreader->nverts;
  }
  vertices = allocVertices(0, vertices_alloced);

  if (psreader->nfaces == -1)
  {
    fprintf(stderr,"nfaces not defined\n");
  }
  else
  {
    fprintf(stderr,"nfaces %d\n",psreader->nfaces);
    faces_alloced = psreader->nfaces;
  }
  faces = allocFaces(0, faces_alloced);

  int i;

  while(psreader->read_triangle())
  {
    // make sure there is memory for the next face
    if (faces_number == faces_alloced)
    {
      faces_alloced *= 2;
      faces = allocFaces(faces, faces_alloced);
    }

    // copy new face into memory
    VecCopy3iv(&(faces[faces_number*3]), psreader->t_idx);
    faces_number++;

    for (i = 0; i < 3; i++)
    {
      // check for new vertices
      if (PS_IS_NEW_VERTEX(psreader->t_vflag[i]))
      {
        // make sure there is memory for the next vertex
        if (vertices_number == vertices_alloced)
        {
          vertices_alloced *= 2;
          vertices = allocVertices(vertices, vertices_alloced);
        }

        // copy new face into memory
        VecCopy3fv(&(vertices[vertices_number*3]), psreader->t_pos_f[i]);
        vertices_number++;
      }
    }
  }

  fprintf(stderr,"vertex counters: %d %d %d (should be equal)\n",vertices_number,psreader->v_count,psreader->nverts);
  fprintf(stderr,"face counters: %d %d %d (should be equal)\n",faces_number,psreader->f_count,psreader->nfaces);

  psreader->close();

  deallocVertices(vertices);
  deallocFaces(faces);

  return 1;
}
