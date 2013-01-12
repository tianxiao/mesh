/*
===============================================================================

  FILE:  psreader_oocc.h
  
  CONTENTS:
  
    Reads a mesh from our oocc compressed format (SIGGRAPH 2003) and provides
    access to it in form of a Processing Sequence.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    21 December 2004 -- make the new t_idx_orig field point to the t_idx field
    13 January 2004 -- fill in the new t_orig field during read_triangle
    02 September 2003 -- changed into an instance of a psreader interface
    16 August 2003 -- added support for per edge user data 
    22 March 2003 -- updated based on Peter's input
    03 March 2003 -- created initial version (psmesh) while waiting at IAD airport
  
===============================================================================
*/
#ifndef PSREADER_OOCC_H
#define PSREADER_OOCC_H

#include "psreader.h"

#include <stdio.h>

class PSreader_oocc : public PSreader
{
public:

  // additional psreader_oocc triangle variables

  int* t_pos_i[3];

  // additional psreader_oocc mesh variables

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

  // psreader_oocc functions

  bool open(const char* file_name);
  bool open(FILE* fp);

  PSreader_oocc();
  ~PSreader_oocc();
};

#endif
