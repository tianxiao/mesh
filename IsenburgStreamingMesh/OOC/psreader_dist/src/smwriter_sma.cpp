/*
===============================================================================

  FILE:  SMwriter_sma.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2003  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "smwriter_sma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec3fv.h"
#include "vec3iv.h"

void SMwriter_sma::add_comment(const char* comment)
{
  if (comments == 0)
  {
    ncomments = 0;
    comments = (char**)malloc(sizeof(char*)*10);
    comments[9] = (char*)-1;
  }
  else if (comments[ncomments] == (char*)-1)
  {
    comments = (char**)realloc(comments,sizeof(char*)*ncomments*2);
    comments[ncomments*2-1] = (char*)-1;
  }
  comments[ncomments] = strdup(comment);
  ncomments++;
}

void SMwriter_sma::set_nverts(int nverts)
{
  this->nverts = nverts;
}

void SMwriter_sma::set_nfaces(int nfaces)
{
  this->nfaces = nfaces;
}

void SMwriter_sma::set_boundingbox(const float* bb_min_f, const float* bb_max_f)
{
  if (this->bb_min_f == 0) this->bb_min_f = new float[3];
  if (this->bb_max_f == 0) this->bb_max_f = new float[3];
  VecCopy3fv(this->bb_min_f, bb_min_f);
  VecCopy3fv(this->bb_max_f, bb_max_f);
}

bool SMwriter_sma::open(FILE* file)
{
  if (file == 0)
  {
    fprintf(stderr, "ERROR: zero file pointer not supported by SMwriter_sma\n");
    exit(0);
  }
  this->file = file;
  v_count = 0;
  f_count = 0;
  return true;
}

void SMwriter_sma::write_vertex(const float* v_pos_f)
{
  if (v_count + f_count == 0) write_header();

  fprintf(file, "v %f %f %f\012",v_pos_f[0],v_pos_f[1],v_pos_f[2]);
  v_count++;
}

void SMwriter_sma::write_triangle(const int* t_idx)
{
  if (v_count + f_count == 0) write_header();

  fprintf(file, "f %d %d %d\012",t_idx[0]+1,t_idx[1]+1,t_idx[2]+1);
  f_count++;
}

void SMwriter_sma::write_triangle(const int* t_idx, const bool* t_final)
{
  if (v_count + f_count == 0) write_header();

  fprintf(file, "f %d %d %d\012",(t_final[0] ? t_idx[0]-v_count : t_idx[0]+1),(t_final[1] ? t_idx[1]-v_count : t_idx[1]+1), (t_final[2] ? t_idx[2]-v_count : t_idx[2]+1));
  f_count++;
}

void SMwriter_sma::write_finalized(int final_idx)
{
  if (final_idx < 0)
  {
    fprintf(file, "x %d\012",final_idx);
  }
  else
  {
    fprintf(file, "x %d\012",final_idx+1);
  }
}

void SMwriter_sma::close()
{
  file = 0;

  if (comments)
  {
    for (int i = 0; i < ncomments; i++)
    {
      free(comments[i]);
    }
    free(comments);
    ncomments = 0;
    comments = 0;
  }

  if (nverts != -1) if (nverts != v_count)  fprintf(stderr,"WARNING: set nverts %d but v_count %d\n",nverts,v_count);
  if (nfaces != -1) if (nfaces != f_count)  fprintf(stderr,"WARNING: set nfaces %d but f_count %d\n",nfaces,f_count);

  v_count = -1;
  f_count = -1;
}

void SMwriter_sma::write_header()
{
  if (comments)
  {
    for (int i = 0; i < ncomments; i++)
    {
      fprintf(file, "# %s\012",comments[i]);
    }
  }
  if (nverts != -1) fprintf(file, "# nverts %d\012",nverts);
  if (nfaces != -1) fprintf(file, "# nfaces %d\012",nfaces);
  if (bb_min_f) fprintf(file, "# bb_min %f %f %f\012",bb_min_f[0],bb_min_f[1],bb_min_f[2]);
  if (bb_max_f) fprintf(file, "# bb_max %f %f %f\012",bb_max_f[0],bb_max_f[1],bb_max_f[2]);
}

SMwriter_sma::SMwriter_sma()
{
  // init of SMwriter interface
  ncomments = 0;
  comments = 0;

  nverts = -1;
  nfaces = -1;

  v_count = -1;
  f_count = -1;

  bb_min_f = 0;
  bb_max_f = 0;

  // init of SMwriter_sma interface
  file = 0;
}

SMwriter_sma::~SMwriter_sma()
{
  // clean-up for SMwriter interface
  if (comments)
  {
    for (int i = 0; i < ncomments; i++)
    {
      free(comments[i]);
    }
    free(comments);
  }
  if (bb_min_f) delete [] bb_min_f;
  if (bb_max_f) delete [] bb_max_f;
}
