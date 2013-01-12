/*
===============================================================================

  FILE:  SMwriter_smb.h
  
  CONTENTS:
  
    Writes a Streaming Mesh in an efficient binary format. 
    
    Optionally the connectivity and the geometry may be compressed using a
    simple lossless format that preserves both, the exact ordering of the
    mesh elements and the rotation of the triangles. But this is not yet
    implemented.

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2004  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    31 July 2004 -- initial version created after a missed Sushi dinner
  
===============================================================================
*/
#ifndef SMWRITER_SMB_H
#define SMWRITER_SMB_H

#include "smwriter.h"

#include <stdio.h>

class SMwriter_smb : public SMwriter
{
public:

  // smwriter interface function implementations

  void add_comment(const char* comment);

  void set_nverts(int nverts);
  void set_nfaces(int nfaces);
  void set_boundingbox(const float* bb_min_f, const float* bb_max_f);

  void write_vertex(const float* v_pos_f);
  void write_triangle(const int* t_idx, const bool* t_final);
  void write_triangle(const int* t_idx);
  void write_finalized(int final_idx);

  void close();

  // smwriter_smb functions

  void set_endianness(bool big_endian);

  bool open(FILE* file);

  SMwriter_smb();
  ~SMwriter_smb();

private:
  FILE* file;

  void write_header();
  void write_buffer();
  void write_buffer_remaining();

  bool endian_swap;

  int element_number;
  unsigned int element_descriptor;
  int* element_buffer;
};

#endif
