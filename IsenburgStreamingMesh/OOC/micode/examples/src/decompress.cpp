/*
===============================================================================

  FILE:  decompress.cpp
  
  CONTENTS:
  
    A demo program that shows how to use the micode mesh compression library.
 
    The program takes as input a compressed indexed mesh and outputs it as a
    indexed mesh in binary PLY format.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2001-2004  martin isenburg@cs.unc.edu
    
    This software is distributed for evaluation purposes only
    WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    14 March 2004 -- created
  
===============================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "micode.h"

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"decompress -i mesh_compressed.ply -o mesh.ply\n");
  fprintf(stderr,"decompress -i mesh_compressed_12bit.ply\n");
  fprintf(stderr,"decompress -h\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  int i;
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
    else
    {
      usage();
    }
  }

  if (file_name_in == 0)
  {
    usage();
  }

  miCodec* micodec = loadCodec(file_name_in);

  if (micodec == 0)
  {
    fprintf(stderr,"ERROR: failed to load micodec from %s\n", file_name_in);
    exit(1);
  }

  miMesh* mimesh = decompress(micodec);

  if (mimesh == 0)
  {
    fprintf(stderr,"ERROR: failed to decompress mesh\n");
    exit(1);
  }

  if (file_name_out)
  {
    if (saveMesh(mimesh, file_name_out) == 0)
    {
      fprintf(stderr,"ERROR: failed to save mesh as %s\n", file_name_out);
      exit(1);
    }
  }

  return 1;
}
