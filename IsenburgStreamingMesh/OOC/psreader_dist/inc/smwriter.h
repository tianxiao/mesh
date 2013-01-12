/*
===============================================================================

  FILE:  SMwriter.h
  
  CONTENTS:
  
    Streaming Mesh Writer
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    17 January 2004 -- added virtual destructor to shut up the g++ compiler
    15 September 2003 -- initial version created on the Monday after a tough
                         hiking weekend in Yosemite
  
===============================================================================
*/
#ifndef SMWRITER_H
#define SMWRITER_H

class SMwriter
{
public:

  // mesh variables

  int ncomments;
  char** comments;

  int nverts;
  int nfaces;

  int v_count;
  int f_count;

  float* bb_min_f;
  float* bb_max_f;

  // functions

  virtual void add_comment(const char* comment)=0;

  virtual void set_nverts(int nverts)=0;
  virtual void set_nfaces(int nfaces)=0;
  virtual void set_boundingbox(const float* bb_min_f, const float* bb_max_f)=0;

  virtual void write_vertex(const float* v_pos_f)=0;
  virtual void write_triangle(const int* t_idx, const bool* t_final)=0;
  virtual void write_triangle(const int* t_idx)=0;
  virtual void write_finalized(int final_idx)=0;

  virtual void close()=0;

  virtual ~SMwriter(){};
};

#endif
