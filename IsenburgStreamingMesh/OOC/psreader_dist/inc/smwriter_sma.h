/*
===============================================================================

  FILE:  SMwriter_sma.h
  
  CONTENTS:
  
    Writes a Streaming Mesh in ASCII format. Optionally the mesh indices may
    be bandwidth-relative or cutwidth-relative (pre-order only).
    When the mesh is written "bandwidth-relative" all indices are expressed as
    the difference to the highest index.
    When the mesh is written "cutwidth-relative" then indices are re-used such
    that the maximal index used equals the cutwidth.
    
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    02 October 2003 -- initial version created on the Thursday that Germany
                       beat Russia 7:1 in the Women Soccer Worlcup
  
===============================================================================
*/
#ifndef SMWRITER_SMA_H
#define SMWRITER_SMA_H

#include "smwriter.h"

#include <stdio.h>

class SMwriter_sma : public SMwriter
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

  // smwriter_sma functions

  bool open(FILE* file);

  SMwriter_sma();
  ~SMwriter_sma();

private:
  FILE* file;
  void write_header();
};

#endif
