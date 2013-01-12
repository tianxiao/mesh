/*
===============================================================================

  FILE:  SMwriter_smb.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2004  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "smwriter_smb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec3fv.h"
#include "vec3iv.h"

#define SM_VERSION 0 // this is SMB

bool SMwriter_smb::open(FILE* file)
{
  if (file == 0)
  {
    fprintf(stderr, "ERROR: zero file pointer not supported by SMwriter_smb\n");
    exit(0);
  }

  this->file = file;
  // write version
  fputc(SM_VERSION, file);

  v_count = 0;
  f_count = 0;

  element_number = 0;
  element_descriptor = 0;

  return true;
}

void SMwriter_smb::close()
{
  write_buffer_remaining();

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

void SMwriter_smb::add_comment(const char* comment)
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

void SMwriter_smb::set_nverts(int nverts)
{
  this->nverts = nverts;
}

void SMwriter_smb::set_nfaces(int nfaces)
{
  this->nfaces = nfaces;
}

void SMwriter_smb::set_boundingbox(const float* bb_min_f, const float* bb_max_f)
{
  if (this->bb_min_f == 0) this->bb_min_f = new float[3];
  if (this->bb_max_f == 0) this->bb_max_f = new float[3];
  VecCopy3fv(this->bb_min_f, bb_min_f);
  VecCopy3fv(this->bb_max_f, bb_max_f);
}

void SMwriter_smb::set_endianness(bool big_endian)
{
#if (defined(i386) || defined(WIN32))   // if little endian machine
  endian_swap = big_endian;
#else                                   // else big endian machine
  endian_swap = !big_endian;
#endif
}

void SMwriter_smb::write_vertex(const float* v_pos_f)
{
  if (v_count + f_count == 0) write_header();

  if (endian_swap) VecCopy3fv_swap_endian((float*)&(element_buffer[element_number*3]), v_pos_f);
  else VecCopy3fv((float*)&(element_buffer[element_number*3]), v_pos_f);
  element_descriptor = 0x80000000 | (element_descriptor >> 1);
  element_number++;

  if (element_number == 32) write_buffer();

  v_count++;
}

void SMwriter_smb::write_triangle(const int* t_idx)
{
  fprintf(stderr, "ERROR: write_triangle(const int* t_idx) not supported by SMwriter_smb\n");
  exit(0);
}

void SMwriter_smb::write_triangle(const int* t_idx, const bool* t_final)
{
  if (v_count + f_count == 0) write_header();

  if (endian_swap) VecSet3iv_swap_endian((int*)&(element_buffer[element_number*3]), (t_final[0] ? t_idx[0]-v_count : t_idx[0]+1),(t_final[1] ? t_idx[1]-v_count : t_idx[1]+1), (t_final[2] ? t_idx[2]-v_count : t_idx[2]+1));
  else VecSet3iv((int*)&(element_buffer[element_number*3]), (t_final[0] ? t_idx[0]-v_count : t_idx[0]+1),(t_final[1] ? t_idx[1]-v_count : t_idx[1]+1), (t_final[2] ? t_idx[2]-v_count : t_idx[2]+1));
  element_descriptor = (element_descriptor >> 1);
  element_number++;

  if (element_number == 32) write_buffer();

  f_count++;
}

void SMwriter_smb::write_finalized(int final_idx)
{
  fprintf(stderr, "ERROR: write_finalized(int final_idx) not supported by SMwriter_smb\n");
  exit(0);
}

static int swap_endian_int(int input)
{
  int output;
  ((char*)&output)[0] = ((char*)&input)[3];
  ((char*)&output)[1] = ((char*)&input)[2];
  ((char*)&output)[2] = ((char*)&input)[1];
  ((char*)&output)[3] = ((char*)&input)[0];
  return output;
}

static unsigned int swap_endian_uint(unsigned int input)
{
  int output;
  ((char*)&output)[0] = ((char*)&input)[3];
  ((char*)&output)[1] = ((char*)&input)[2];
  ((char*)&output)[2] = ((char*)&input)[1];
  ((char*)&output)[3] = ((char*)&input)[0];
  return output;
}

#define SM_LITTLE_ENDIAN 0
#define SM_BIG_ENDIAN 1
#define SM_COMPRESSION 0

void SMwriter_smb::write_header()
{
  int output;
  // endianness
#if (defined(i386) || defined(WIN32))   // if little endian machine
  if (endian_swap) fputc(SM_BIG_ENDIAN, file);
  else fputc(SM_LITTLE_ENDIAN, file);
#else                                    // else big endian machine
  if (endian_swap) fputc(SM_LITTLE_ENDIAN, file);
  else fputc(SM_BIG_ENDIAN, file);
#endif
  // compression
  fputc(SM_COMPRESSION, file);
  fputc(SM_COMPRESSION, file);
  // write comments
  if (endian_swap) output = swap_endian_int(ncomments);
  else output = ncomments;
  fwrite(&output, sizeof(int), 1, file);
  if (ncomments)
  {
    for (int i = 0; i < ncomments; i++)
    {
      if (endian_swap) output = swap_endian_int(strlen(comments[i]));
      else output = strlen(comments[i]);
      fwrite(&output, sizeof(int), 1, file);
      fwrite(comments[i], sizeof(char), output, file);
    }
  }
  // write nverts
  if (endian_swap) output = swap_endian_int(nverts);
  else output = nverts;
  fwrite(&output, sizeof(int), 1, file);
  // write nfaces
  if (endian_swap) output = swap_endian_int(nfaces);
  else output = nfaces;
  fwrite(&output, sizeof(int), 1, file);
  // write bounding box
  if (bb_min_f && bb_max_f)
  {
    fputc(1, file);
    if (endian_swap)
    {
      float temp[3];
      VecCopy3fv_swap_endian(temp, bb_min_f);
      fwrite(temp, sizeof(float), 3, file);
      VecCopy3fv_swap_endian(temp, bb_max_f);
      fwrite(temp, sizeof(float), 3, file);
    }
    else
    {
      fwrite(bb_min_f, sizeof(float), 3, file);
      fwrite(bb_max_f, sizeof(float), 3, file);
    }
  }
  else
  {
    fputc(0, file);
  }
}

void SMwriter_smb::write_buffer()
{
  if (endian_swap) element_descriptor = swap_endian_uint(element_descriptor);
  fwrite(&element_descriptor, sizeof(unsigned int), 1, file);
  element_descriptor = 0;
  fwrite(element_buffer, sizeof(int), 32*3, file);
  element_number = 0;
}

void SMwriter_smb::write_buffer_remaining()
{
  element_descriptor = element_descriptor >> (32 - element_number);
  if (endian_swap) element_descriptor = swap_endian_uint(element_descriptor);
  fwrite(&element_descriptor, sizeof(unsigned int), 1, file);
  element_descriptor = 0;
  fwrite(element_buffer, sizeof(int), element_number*3, file);
  element_number = 0;
}

SMwriter_smb::SMwriter_smb()
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

  // init of SMwriter_smb interface
  file = 0;
  element_buffer = (int*)malloc(sizeof(int)*3*32);
  endian_swap = false;
}

SMwriter_smb::~SMwriter_smb()
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

  // clean-up for SMwriter_smb interface
  free(element_buffer);
}
