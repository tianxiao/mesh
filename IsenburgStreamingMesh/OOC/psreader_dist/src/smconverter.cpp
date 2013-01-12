/*
===============================================================================

  FILE:  SMconverter.cpp
  
  CONTENTS:
  
    Reads a mesh from a Processing Sequence and provides access to it in form
    of a Streaming Mesh. While this surely is a reduction of functionality it
    may still be a useful tool to have. For example, one may want to access an
    out-of-core compressed (SIGGRAPH 2003) mesh as a Streaming Mesh.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    02 October 2003 -- added functionality for read_element() 
    15 September 2003 -- initial version created on the Monday after a tough
                         hiking weekend in Yosemite
  
===============================================================================
*/
#include "smconverter.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vec3fv.h"
#include "vec3iv.h"

bool SMconverter::open(PSreader* psreader)
{
  this->psreader = psreader;

  nverts = psreader->nverts;
  nfaces = psreader->nfaces;

  if (psreader->bb_min_f)
  {
    if (bb_min_f == 0) bb_min_f = new float[3];
    VecCopy3fv(bb_min_f, psreader->bb_min_f);
  }
  if (psreader->bb_max_f)
  {
    if (bb_max_f == 0) bb_max_f = new float[3];
    VecCopy3fv(bb_max_f, psreader->bb_max_f);
  }

  v_count = 0;
  f_count = 0;

  have_new = 0; next_new = 0;
  have_triangle = 0;
  have_finalized = 0; next_finalized = 0;

  return true;
}

void SMconverter::close()
{
  nverts = -1;
  nfaces = -1;

  v_count = -1;
  f_count = -1;

  if (bb_min_f) {delete [] bb_min_f; bb_min_f = 0;}
  if (bb_max_f) {delete [] bb_max_f; bb_max_f = 0;}
}

SMevent SMconverter::read_element()
{
  if ((have_new + have_triangle) == 0)
  {
    next_new = 0;
    have_finalized = next_finalized = 0;
    if (psreader->read_triangle())
    {
      for (int i = 0; i < 3; i++)
      {
        if (PS_IS_NEW_VERTEX(psreader->t_vflag[i]))
        {
          new_vertices[have_new] = i;
          have_new++;
        }
        if (PS_IS_FINALIZED_VERTEX(psreader->t_vflag[i]))
        {
          t_final[i] = true;
          finalized_vertices[have_finalized] = i;
          have_finalized++;
        }
        else
        {
          t_final[i] = false;
        }
      }
      have_triangle = 1;
    }
    else
    {
      if (nverts == -1)
      {
        nverts = v_count;
      }
      else
      {
        if (v_count != nverts)
        {
          fprintf(stderr,"ERROR: wrong vertex count: v_count (%d) != nverts (%d)\n", v_count, nverts);
        }
      }
      if (nfaces == -1)
      {
        nfaces = f_count;
      }
      else
      {
        if (f_count != nfaces)
        {
          fprintf(stderr,"ERROR: wrong face count: f_count (%d) != nfaces (%d)\n", f_count, nfaces);
        }
      }
      return SM_EOF;
    }
  }
  if (have_new)
  {
    v_idx = psreader->t_idx[new_vertices[next_new]];
    VecCopy3fv(v_pos_f, psreader->t_pos_f[new_vertices[next_new]]);
    have_new--; next_new++;
    v_count++;
    return SM_VERTEX;
  }
  else if (have_triangle)
  {
    VecCopy3iv(t_idx, psreader->t_idx);
    have_triangle = 0;
    f_count++;
    return SM_TRIANGLE;
  }
  return SM_ERROR;
}

SMevent SMconverter::read_event()
{
  if (have_finalized == 0)
  {
    return read_element();
  }
  else
  {
    final_idx = psreader->t_idx[finalized_vertices[next_finalized]];
    have_finalized--; next_finalized++;
    return SM_FINALIZED;
  }
  return SM_ERROR;
}

SMconverter::SMconverter()
{
  // init of SMreader interface
  ncomments = 0;
  comments = 0;

  nverts = -1;
  nfaces = -1;

  v_count = -1;
  f_count = -1;

  bb_min_f = 0;
  bb_max_f = 0;

  post_order = false;
}

SMconverter::~SMconverter()
{
}
