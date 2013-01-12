/*
===============================================================================

  FILE:  psreader_lowspan.h
  
  CONTENTS:
  
    Reads a mesh from our a format similar to the oocc-compressed (SIGGRAPH 2003)
    and provides access to it in form of a Processing Sequence. The main difference
    is that these are breadth-first traversed during compression.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    21 December 2004 -- make the new t_idx_orig field point to the t_idx field
    29 December 2003 -- created when Pam's christmas goodies went stale
  
===============================================================================
*/
#ifndef PSREADER_LOWSPAN_H
#define PSREADER_LOWSPAN_H

#include "psreader.h"

#include <stdio.h>

class PSreader_lowspan : public PSreader
{
public:

  // additional PSreader_lowspan triangle variables

  int* t_pos_i[3];

  // additional PSreader_lowspan mesh variables

  int bits;

  int* bb_min_i;
  int* bb_max_i;

  // psreader interface function implementations

  PSevent read_triangle();

  void set_vdata(const void* data, int i);
  void* get_vdata(int i) const;

  void set_edata(const void* data, int i);
  void* get_edata(int i) const;

  void close();

  // PSreader_lowspan functions

  bool open(const char* file_name);
  bool open(FILE* fp);

  PSreader_lowspan();
  ~PSreader_lowspan();
};

#endif
