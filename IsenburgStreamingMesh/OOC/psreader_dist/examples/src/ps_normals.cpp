/*
===============================================================================

  FILE:  ps_normals.cpp
  
  CONTENTS:
  
    A demo program that uses the Processing Sequence API. Its main purpose
    is to provide other programmers an easy-to-follow example of how the
    Processing Sequence API can be used for some mesh processing task.

    This simple program computes smooth normals (e.g. per vertex normals) for
    the mesh. These normals could then be used for rendering (*). Here we only
    report a rough distribution of these normals.
    
    (*) Note that this would require us to temporarily buffer triangles until
    the computation of smooth normal was completed for all three vertices.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    11 September 2003 -- created initial version just after midnight 
  
===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psreader_oocc.h"
#include "psreader_lowspan.h"
#include "psconverter.h"
#include "smreader_sma.h"
#include "smreader_smb.h"
#include "smreader_smc.h"

#include "vec3fv.h"

typedef struct Normal
{
  float value[3];
  int use;
} Normal;

Normal* allocNormal()
{
  Normal* normal = (Normal*)malloc(sizeof(Normal));
  if (normal == 0)
  {
    printf("ERROR: malloc for normal failed.\n");
    exit(-1);
  }
  normal->value[0] = 0.0f;
  normal->value[1] = 0.0f;
  normal->value[2] = 0.0f;
  normal->use = 0;
  return normal;
}

void deallocNormal(Normal* normal)
{
  free(normal);
}

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

int main(int argc, char *argv[])
{
  char* file_name;

  if (argc == 2)
  {
    file_name = argv[1];
  }
  else
  {
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"sm_normals <file_name>\n");
    exit(1);
  }

  PSreader* psreader = 0;

  if (strstr(file_name, ".sma") || strstr(file_name, ".obj") || strstr(file_name, ".smf"))
  {
    FILE* file;
    if (strstr(file_name, ".gz"))
    {
      #ifdef _WIN32
      file = fopenGzipped(file_name, "r");
      #else
      fprintf(stderr,"ERROR: cannot open gzipped file %s\n",file_name);
      exit(1);
      #endif
    }
    else
    {
      file = fopen(file_name, "r");
    }
    if (file == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      exit(1);
    }
    SMreader_sma* smreader_sma = new SMreader_sma();
    smreader_sma->open(file);
    PSconverter* psconverter = new PSconverter();
    psconverter->open(smreader_sma, 256, 512);
    psreader = psconverter;
  }
  else if (strstr(file_name, ".smb"))
  {
    FILE* file = fopen(file_name, "rb");
    if (file == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      exit(1);
    }
    SMreader_smb* smreader_smb = new SMreader_smb();
    smreader_smb->open(file);
    PSconverter* psconverter = new PSconverter();
    psconverter->open(smreader_smb, 256, 512);
    psreader = psconverter;
  }
  else if (strstr(file_name, ".smc") || strstr(file_name, ".sme"))
  {
    FILE* file = fopen(file_name, "rb");
    if (file == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      exit(1);
    }
    SMreader_smc* smreader_smc = new SMreader_smc();
    smreader_smc->open(file);
    PSconverter* psconverter = new PSconverter();
    psconverter->open(smreader_smc, 256, 512);
    psreader = psconverter;
  }
  else if (strstr(file_name, "_compressed") || strstr(file_name, "depthfirst") || strstr(file_name, "depth_first"))
  {
    PSreader_oocc* psreader_oocc = new PSreader_oocc();
    psreader_oocc->open(file_name);
    psreader = psreader_oocc;
  }
  else if (strstr(file_name, "_lowspan") || strstr(file_name, "breadthfirst") || strstr(file_name, "breadth_first"))
  {
    PSreader_lowspan* psreader_lowspan = new PSreader_lowspan();
    psreader_lowspan->open(file_name);
    psreader = psreader_lowspan;
  }
  else
  {
    fprintf(stderr,"ERROR: cannot guess input type from file name\n");
    exit(1);
  }

  int i;
  float tmp[3];
  float length;
  Normal* normal;

  int alloced_normals = 0;
  int max_alloced_normals = 0;

  int above_xy_plane = 0;
  int below_xy_plane = 0;
  int inside_xy_plane = 0;
  int zero_normal = 0;

  int max_normal_use = 0;

  while(psreader->read_triangle())
  {
    // compute an (unnormalized) normal for the triangle
    VecCcwNormal3fv(tmp, psreader->t_pos_f[0], psreader->t_pos_f[1], psreader->t_pos_f[2]);

    // add normal to average normal of all three vertices
    for (i = 0; i < 3; i++)
    {
      // get average normal of vertex
      if (PS_IS_NEW_VERTEX(psreader->t_vflag[i]))
      {
        normal = allocNormal();                     // create new normal
        psreader->set_vdata((void*)normal, i);
        alloced_normals++;
        if (alloced_normals > max_alloced_normals) max_alloced_normals = alloced_normals;
      }
      else
      {
        normal = (Normal*)psreader->get_vdata(i);    // use old normal
      }

      // add normal to average normal
      VecSelfAdd3fv(normal->value, tmp);
      normal->use++;

      // compute smooth normals and some statistics for finalized vertices & deallocate them
      if (PS_IS_FINALIZED_VERTEX(psreader->t_vflag[i]))
      {
        length = VecLength3fv(normal->value);
        if (length > 1e-10f)
        {
          VecSelfScalarDiv3fv(normal->value, length);
          if (normal->value[2] > 1e-10f)
          {
            above_xy_plane++;
          }
          else if (normal->value[2] < -1e-10f)
          {
            below_xy_plane++;
          }
          else
          {
            inside_xy_plane++;
          }
        }
        else
        {
          zero_normal++;
        }
        if (normal->use > max_normal_use) max_normal_use = normal->use;
        deallocNormal(normal);
        alloced_normals--;
      }
    }
  }

  fprintf(stderr,"number of vertices: %d\n",psreader->nverts);
  fprintf(stderr,"number of faces: %d\n",psreader->nfaces);

  psreader->close();

  fprintf(stderr,"number of smooth normals:\n");
  fprintf(stderr,"  above the xy-plane:    %d\n", above_xy_plane);
  fprintf(stderr,"  below the xy-plane:    %d\n", below_xy_plane);
  fprintf(stderr,"  inside the xy-plane:   %d\n", inside_xy_plane);
  fprintf(stderr,"  of (near) zero length: %d\n", zero_normal);
  fprintf(stderr,"maximum number of corners using the same normal: %d\n", max_normal_use);
  fprintf(stderr,"maximum number of concurrently allocated normals: %d\n", max_alloced_normals);

  return 1;
}
