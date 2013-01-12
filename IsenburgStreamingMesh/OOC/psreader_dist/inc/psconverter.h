/*
===============================================================================

  FILE:  PSconverter.h
  
  CONTENTS:
  
    The Processing Sequence Converter:

    It.*on-the-fly* converts a streaming mesh into a processing sequence.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    21 March 2005 -- fixed a bug in set_vdata() for non-manifold vertices
    21 December 2004 -- moved t_idx_orig to PSreader.h interface
    16 January 2004 -- fixed a mean memory bug under SIG04 deadline stress
    13 January 2004 -- moved t_orig to PSreader.h interface
    8 January 2004 -- failure check for too many vertex-adjacent starts
    19 December 2003 -- added t_orig and t_idx_orig (original indices)
    28 October 2003 -- switched from Peter's hash to the STL hash_map
    12 September 2003 -- indices instead of pointers for psov and pscv
    28 August 2003 -- initial version created on a hot Thursday at LLNL
  
===============================================================================
*/
#ifndef PSCONVERTER_H
#define PSCONVERTER_H

#include "psreader.h"
#include "smreader.h"

class PSconverter : public PSreader
{
public:

  // psreader interface function implementations

  PSevent read_triangle();

  void set_vdata(const void* data, int i);
  void* get_vdata(int i) const;

  void set_edata(const void* data, int i);
  void* get_edata(int i) const;

  void close();

  // psreader_converter functions

  bool open(SMreader* smreader, int low=256, int high=512);

  PSconverter();
  ~PSconverter();

private:
  SMreader* smreader;

  int process_event();
  void fill_output_buffer();
  void process_finalized_vertex(int pscv_idx);

  // efficient memory allocation for triangles
  int init_triangle_buffer(int size);
  int alloc_triangle();
  void dealloc_triangle(int te0);

  // efficient memory allocation for connectivity vertices
  int init_connectivity_vertex_buffer(int size);
  int alloc_connectivity_vertex();
  void dealloc_connectivity_vertex(int pscv_idx);

  // efficient memory allocation for output vertices
  int init_output_vertex_buffer(int size);
  int alloc_output_vertex();
  void dealloc_output_vertex(int psov_idx);
};

#endif
