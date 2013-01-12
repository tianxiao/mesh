/*
===============================================================================

  FILE:  compress.cpp
  
  CONTENTS:
  
    A demo program that shows how to use the micode mesh compression library.
 
    The program takes as input an indexed mesh in binary PLY format and outputs
    it as a compressed indexed mesh
  
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

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"compress -i mesh.ply -o mesh_compressed_16bit.ply\n");
  fprintf(stderr,"compress -i mesh.ply.gz -b 12 -o mesh_compressed_12bit.ply\n");
  fprintf(stderr,"compress -i mesh.ply \n");
  fprintf(stderr,"compress -h\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  int i;
  int bits = 16;
  char* file_name_in = 0;
  char* file_name_out = 0;

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name_in = argv[i];
    }
    else if (strcmp(argv[i],"-o") == 0)
    {
      i++;
      file_name_out = argv[i];
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

  miCodec* micodec = compress(mimesh, bits);

  if (micodec == 0)
  {
    fprintf(stderr,"ERROR: failed to compress mesh\n");
    exit(1);
  }

  if (file_name_out)
  {
    if (saveCodec(micodec, file_name_out) == 0)
    {
      fprintf(stderr,"ERROR: failed to save codec as %s\n", file_name_out);
      exit(1);
    }
  }

  return 1;
}
