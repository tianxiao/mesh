/*
===============================================================================

  FILE:  micode.h
  
  CONTENTS:
  
    a library for compression and decompression of - optionally non-manifold -
    indexed triangles meshes
  
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
#ifndef MICODE_H
#define MICODE_H

// for handing over mesh

typedef struct miMesh
{
  int nverts;
  float* vertices;  // for efficieny the compressor uses (e.g. corrupts) this array
  int nfaces;
  int* faces;       // for efficieny the compressor uses (e.g. corrupts) this array
} miMesh;

// for handing over codec

typedef struct miCodec
{
  int nverts;
  int nfaces;
  int bits;
  float bb_min[3];
  float bb_max[3];
  int nbytes;
  unsigned char* bytes;
} miCodec;

// core functionality

miCodec* compress(miMesh* mimesh, int bits=16, const float* bb_min=0, const float* bb_max=0);

miMesh* decompress(miCodec* codec);

// helpful utilities

miMesh* loadMesh(const char* file_name);
int saveMesh(miMesh* mimesh, const char* file_name);

miCodec* loadCodec(const char* file_name);
int saveCodec(miCodec* micodec, const char* file_name);

// allows you to check how quantization affects your data. if no explicit bounding box info
// is provided (e.g. bb_min & bb_max) a bounding box is computed from the vertices array.

int quantize(int nverts, float* vertices, int bits, const float* bb_min=0, const float* bb_max=0);

#endif
