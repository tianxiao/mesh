/*
===============================================================================

  FILE:  sm_info.cpp
  
  CONTENTS:
  
    This program inputs a streaming mesh and outputs several statistics.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2005  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    19 April 2005 -- changed to compute the triangle width instead
    14 April 2005 -- created after endless discussions about vskip and tskip
  
===============================================================================
*/
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <hash_map.h>

#include "smreader_sma.h"
#include "smreader_smb.h"
#include "smreader_smc.h"
#include "smreader_smd.h"
#include "smreader_ply.h"

#include "smreadpreascompactpre.h"
#include "smreadpostascompactpre.h"

#include "vec3iv.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
extern "C" int gettime_in_msec();
extern "C" int gettime_in_sec();
extern "C" void settime();
#endif

// efficient memory allocation

typedef struct SMvertex
{
  SMvertex* buffer_next;    // used for efficient memory management and by dynamicvector data structure
  SMvertex* buffer_prev;    // used for efficient memory management and by dynamicvector data structure
  int count;
} SMvertex;

static int vertex_buffer_size = 0;
static int vertex_buffer_alloc = 512;
static SMvertex* vertex_buffer_next = 0;
static int vertex_buffer_maxsize = 0;

static SMvertex* allocVertex()
{
  if (vertex_buffer_next == 0)
  {
    vertex_buffer_next = (SMvertex*)malloc(sizeof(SMvertex)*vertex_buffer_alloc);
    if (vertex_buffer_next == 0)
    {
      fprintf(stderr,"malloc for vertex buffer failed\n");
      return 0;
    }
    for (int i = 0; i < vertex_buffer_alloc; i++)
    {
      vertex_buffer_next[i].buffer_next = &(vertex_buffer_next[i+1]);
    }
    vertex_buffer_next[vertex_buffer_alloc-1].buffer_next = 0;
    vertex_buffer_alloc = 2*vertex_buffer_alloc;
  }
  // get pointer to next available vertex
  SMvertex* vertex = vertex_buffer_next;
  vertex_buffer_next = vertex->buffer_next;

  vertex_buffer_size++;
  if (vertex_buffer_size > vertex_buffer_maxsize) vertex_buffer_maxsize = vertex_buffer_size;

  return vertex;
}

static void deallocVertex(SMvertex* vertex)
{
  vertex->buffer_next = vertex_buffer_next;
  vertex_buffer_next = vertex;
  vertex_buffer_size--;
}

typedef hash_map<int, SMvertex*> my_vertex_hash;

static my_vertex_hash* vertex_hash;

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"sm_info mesh.smb \n");
  fprintf(stderr,"sm2sm -h\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  int i;
  bool dry = 0;
  bool isma = 0;
  bool ismb = 0;
  bool ismc = 0;
  bool ismd = 0;
  bool isme = 0;
  bool compact = 0;
  char* file_name = 0;

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-isma") == 0)
    {
      isma = true;
    }
    else if (strcmp(argv[i],"-ismb") == 0)
    {
      ismb = true;
    }
    else if (strcmp(argv[i],"-ismc") == 0)
    {
      ismc = true;
    }
    else if (strcmp(argv[i],"-ismd") == 0)
    {
      ismd = true;
    }
    else if (strcmp(argv[i],"-isme") == 0)
    {
      isme = true;
    }
    else if (strcmp(argv[i],"-compact") == 0)
    {
      compact = true;
    }
    else if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name = argv[i];
    }
    else if (i == argc-1)
    {
      file_name = argv[argc-1];
    }
  }

  SMreader* smreader;
  FILE* file;
  
  if (file_name)
  {
    if (strstr(file_name, ".gz"))
    {
#ifdef _WIN32
      if (strstr(file_name, ".sma.gz"))
      {
        file = fopenGzipped(file_name, "r");
      }
      else if (strstr(file_name, ".smb.gz") || strstr(file_name, ".smc.gz") || strstr(file_name, ".smd.gz") || strstr(file_name, ".sme.gz") || strstr(file_name, ".ply.gz"))
      {
        file = fopenGzipped(file_name, "rb");
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma.gz .smb.gz .smc.gz or .smd.gz\n",file_name);
        exit(0);
      }
#else
      fprintf(stderr,"ERROR: cannot open gzipped file '%s'\n",file_name);
      exit(1);
#endif
    }
    else
    {
      if (strstr(file_name, ".sma"))
      {
        file = fopen(file_name, "r");
        fprintf(stderr,"opening '%s'\n",file_name);
      }
      else if (strstr(file_name, ".smb") || strstr(file_name, ".smc") || strstr(file_name, ".smd") || strstr(file_name, ".sme"))
      {
        file = fopen(file_name, "rb");
        fprintf(stderr,"opening '%s'\n",file_name);
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma .smb .smc or .smd\n",file_name);
        exit(0);
      }
    }
  }
  else
  {
    if (isma || ismb || ismc || ismd || isme)
    {
      file = stdin;
#ifdef _WIN32
      if (isma)
      {
        if(_setmode( _fileno( stdin ), _O_TEXT ) == -1 )
        {
          fprintf(stderr, "ERROR: cannot set stdin to text (translated) mode\n");
        }
      }
      else
      {
        if(_setmode( _fileno( stdin ), _O_BINARY ) == -1 )
        {
          fprintf(stderr, "ERROR: cannot set stdin to binary (untranslated) mode\n");
        }
      }
#endif
    }
    else
    {
      fprintf(stderr,"ERROR: no input file format specified\n");
      exit(0);
    }
  }

  if (file == 0)
  {
    fprintf(stderr,"ERROR: cannot open '%s' for read\n", file_name);
    exit(0);
  }
  
  if (file_name)
  {
    if (strstr(file_name, ".sma"))
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file);
      smreader = smreader_sma;
    }
    else if (strstr(file_name, ".smb"))
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file);
      smreader = smreader_smb;
    }
    else if (strstr(file_name, ".smd"))
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file);
      smreader = smreader_smd;
    }
    else if (strstr(file_name, ".smc") || strstr(file_name, ".sme"))
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file);
      smreader = smreader_smc;
    }
    else if (strstr(file_name, ".ply"))
    {
      SMreader_ply* smreader_ply = new SMreader_ply();
      smreader_ply->open(file);
      smreader = smreader_ply;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine which reader to use for '%s'\n", file_name);
      exit(0);
    }
  }
  else
  {
    if (isma)
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file);
      smreader = smreader_sma;
    }
    else if (ismb)
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file);
      smreader = smreader_smb;
    }
    else if (ismd)
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file);
      smreader = smreader_smd;
    }
    else if (ismc || isme)
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file);
      smreader = smreader_smc;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine which reader to use\n");
      exit(0);
    }
  }

  if (compact)
  {
    if (smreader->post_order)
    {
      fprintf(stderr,"WARNING: applying filter PostAsCompactPre...\n");
      SMreadPostAsCompactPre* smreadpostascompactpre = new SMreadPostAsCompactPre();
      smreadpostascompactpre->open(smreader);
      smreader = smreadpostascompactpre;
    }
    else 
    {
      fprintf(stderr,"WARNING: applying filter PreAsCompactPre...\n");
      SMreadPreAsCompactPre* smreadpreascompactpre = new SMreadPreAsCompactPre();
      smreadpreascompactpre->open(smreader);
      smreader = smreadpreascompactpre;
    }
  }

  int event;

#ifdef _WIN32
  settime();
#endif

  int twidth_current = 0;
  int twidth_max = 0;
  double twidth_avg = 0.0;

  SMvertex* first = 0;
  SMvertex* last = 0;

  int min_idx;
  int max_idx;
  int index = 0;
  my_vertex_hash::iterator hash_element;
  SMvertex* vertex;

  vertex_hash = new my_vertex_hash;

  while (event = smreader->read_element())
  {
    switch (event)
    {
    case SM_VERTEX:
      break;
    case SM_TRIANGLE:
      min_idx = VecMin3iv(smreader->t_idx);
      max_idx = VecMax3iv(smreader->t_idx);
      // allocate all vertices until max_idx
      while (index <= max_idx)
      {
        // create new vertex
        vertex = allocVertex();
        vertex->count = 0;
        // add new vertex to list
        if (first == 0)
        {
          first = vertex;
          last = vertex;
          vertex->buffer_prev = 0;
          vertex->buffer_next = 0;
        }
        else
        {
          last->buffer_next = vertex;
          vertex->buffer_prev = last;
          vertex->buffer_next = 0;
          last = vertex;
        }
        // add vertex to hash
        vertex_hash->insert(my_vertex_hash::value_type(index, vertex));
        index++;
      }
      // get the three vertices from the hash
      for (i = 0; i < 3; i++)
      {
        hash_element = vertex_hash->find(smreader->t_idx[i]);
        if (hash_element == vertex_hash->end())
        {
          fprintf(stderr, "ERROR: vertex %d not in hash\n", smreader->t_idx[i]);
        }
        else
        {
          vertex = (*hash_element).second;
          
          if (min_idx == smreader->t_idx[i]) // increase the count for the earliest vertex
          {
            vertex->count++;
          }
          else if (max_idx == smreader->t_idx[i]) // decrease the count for the latest vertex
          {
            vertex->count--;
          }
        }
        if (smreader->t_final[i])
        {
          if (vertex->count == 0)
          {
            if (vertex == first)
            {
              first = vertex->buffer_next;
              if (first) first->buffer_prev = 0;
            }
            else if (vertex == last)
            {
              last = vertex->buffer_prev;
              if (last) last->buffer_next = 0;
            }
            else
            {
              vertex->buffer_prev->buffer_next = vertex->buffer_next;
              vertex->buffer_next->buffer_prev = vertex->buffer_prev;
            }
            deallocVertex(vertex);
          }
          else
          {
            vertex->count -= 1000000000; // cannot delete vertices with count. mark as finalized
          }
          vertex_hash->erase(hash_element);
        }
      }

      while (first && first->count < -500000000)
      {
        first->count += 1000000000; // unmark as finalized
        twidth_current += first->count;
        if (twidth_current > twidth_max) twidth_max = twidth_current;
        vertex = first;
        if (first->buffer_next)
        {
          first = first->buffer_next;
          first->buffer_prev = 0;
        }
        else
        {
          first = 0;
          last = 0;
        }
        deallocVertex(vertex);
      }
      break;
    case SM_FINALIZED:
      hash_element = vertex_hash->find(smreader->final_idx);
      if (hash_element == vertex_hash->end())
      {
        fprintf(stderr,"WARNING: finalized vertex %d not in hash\n",smreader->final_idx);
        exit(0);
      }
      vertex = (*hash_element).second;
      if (vertex->count == 0)
      {
        if (vertex == first)
        {
          first = vertex->buffer_next;
          if (first) first->buffer_prev = 0;
        }
        else if (vertex == last)
        {
          last = vertex->buffer_prev;
          if (last) last->buffer_next = 0;
        }
        else
        {
          vertex->buffer_prev->buffer_next = vertex->buffer_next;
          vertex->buffer_next->buffer_prev = vertex->buffer_prev;
        }
        deallocVertex(vertex);
      }
      else
      {
        vertex->count -= 1000000000; // cannot delete vertices with count. mark as finalized
      }
      vertex_hash->erase(hash_element);

      while (first && first->count < 500000000)
      {
        first->count += 1000000000; // unmark as finalized
        twidth_current += first->count;
        if (twidth_current > twidth_max) twidth_max = twidth_current;
        vertex = first;
        if (first->buffer_next)
        {
          first = first->buffer_next;
          first->buffer_prev = 0;
        }
        else
        {
          first = 0;
          last = 0;
        }
        deallocVertex(vertex);
      }
      break;
    default:
      break;
    }
  }

  while (first)
  {
    twidth_current += first->count;
    if (twidth_current > twidth_max) twidth_max = twidth_current;
    vertex = first;
    if (first->buffer_next)
    {
      first = first->buffer_next;
      first->buffer_prev = 0;
    }
    else
    {
      first = 0;
      last = 0;
    }
    deallocVertex(vertex);
  }

  fprintf(stderr,"v_count %d\n",smreader->v_count);
  fprintf(stderr,"f_count %d\n",smreader->f_count);

#ifdef _WIN32
  fprintf(stderr,"needed %6.3f seconds\n",0.001f*gettime_in_msec());
#endif

  if (vertex_hash->size()) fprintf(stderr,"WARNING: %d unfinalized vertices\n",vertex_hash->size());
  fprintf(stderr,"vertex_buffer_size %d vertex_buffer_maxsize %d\n", vertex_buffer_size, vertex_buffer_maxsize);

  delete vertex_hash;

//  fprintf(stderr,"twidth_current %d twidth_max %d twidth_avg %5.1f\n",twidth_current, twidth_max, 0.0f);
  fprintf(stderr,"twidth_current %d twidth_max %d\n",twidth_current, twidth_max);

  smreader->close();
  if (file && file_name) fclose(file);
  delete smreader;

  return 1;
}
