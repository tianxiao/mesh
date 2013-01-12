/*
===============================================================================

  FILE:  psreader.h
  
  CONTENTS:
  
    Processing Sequence API
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    21 December 2004 -- added t_idx_orig which used to be in PSconverter.h
    17 January 2004 -- added virtual destructor to shut up the g++ compiler
    13 January 2004 -- added t_orig which used to be in PSconverter.h
    16 August 2003 -- added support for per edge user data 
    22 March 2003 -- updated based on Peter's input
    03 March 2003 -- created initial version (psmesh) while waiting at IAD airport
  
===============================================================================
*/
#ifndef PSREADER_H
#define PSREADER_H

// events 
typedef enum {
  PS_ERROR = -1,
  PS_EOF = 0,
  PS_VERTEX = 1,
  PS_TRIANGLE = 2,
  PS_FINALIZED = 3
} PSevent;

// edge and vertex flags
#define PS_UNDEFINED     -1
#define PS_FIRST          1
#define PS_LAST           2
#define PS_BORDER         4
#define PS_NON_MANIFOLD   8
#define PS_NOT_ORIENTED  16

// helper macros for querying the flags
#define PS_IS_ENTERING_EDGE(eflag)       ((eflag & (PS_FIRST|PS_LAST)) == PS_FIRST)
#define PS_IS_STAYING_EDGE(eflag)        (eflag == (PS_NON_MANIFOLD))
#define PS_IS_LEAVING_EDGE(eflag)        ((eflag & (PS_FIRST|PS_LAST)) == PS_LAST)
#define PS_IS_BORDER_EDGE(eflag)         ((eflag & (PS_FIRST|PS_LAST|PS_BORDER)) == (PS_FIRST|PS_LAST|PS_BORDER))
#define PS_IS_NON_MANIFOLD_EDGE(eflag)   (!!(eflag & (PS_NON_MANIFOLD)))
#define PS_IS_NOT_ORIENTED_EDGE(eflag)   (!!(eflag & (PS_NOT_ORIENTED)))

#define PS_IS_NEW_VERTEX(vflag)          (!!(vflag & (PS_FIRST)))
#define PS_IS_FINALIZED_VERTEX(vflag)    (!!(vflag & (PS_LAST)))
#define PS_IS_BORDER_VERTEX(vflag)       (!!(vflag & (PS_BORDER)))
#define PS_IS_NON_MANIFOLD_VERTEX(vflag) (!!(vflag & (PS_NON_MANIFOLD)))

class PSreader
{
public:
  
  // vertex variables

  int    v_idx;
  float* v_pos_f;
  int    v_vflag;

  // triangle variables

  int    t_orig;
  int*   t_idx_orig; // may point to the field t_idx[3]
  int    t_idx[3];
  float* t_pos_f[3];
  int    t_vflag[3];
  int    t_eflag[3];

  // mesh variables

  float* bb_min_f;
  float* bb_max_f;

  int nfaces;
  int nverts;
  int ncomps;

  int f_count;
  int h_count;
  int v_count;
  int c_count;

  int nm_v_count;
  int nm_e_count;

  // functions

  virtual PSevent read_triangle() = 0;

  virtual void set_vdata(const void* data, int i) = 0;
  virtual void* get_vdata(int i) const = 0;

  virtual void set_edata(const void* data, int i) = 0;
  virtual void* get_edata(int i) const = 0;

  virtual void close() = 0;

  virtual ~PSreader(){};
};

#endif
