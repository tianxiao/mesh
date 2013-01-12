/*
===============================================================================

  FILE:  ps_angles.cpp
  
  CONTENTS:
  
    A demo program that uses the Processing Sequence API. Its main purpose
    is to provide other programmers an easy-to-follow example of how the
    Processing Sequence API can be used for some mesh processing task.

    This simple program computes crease angles (e.g. the angular difference in
    normals of adjacent triangles) across all manifold edges of the mesh. Then
    a rough distribution of these crease angles is reported.
  
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

float compute_crease_angle(float* a, float* b)
{
  return (float)(180*acos(VecDotProd3fv(a, b))/3.141592653);
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
    fprintf(stderr,"sm_angles <file_name>\n");
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

  int i,j;
  float tmp[3];
  float length;
  float crease_angle;
  Normal* normal;
  Normal* other_normal;

  int alloced_normals = 0;
  int max_alloced_normals = 0;

  int zero_normal = 0;

  int do_nothing_zero = 0;
  int do_nothing_other_zero = 0;
  int do_nothing_both_zero = 0;
  int do_nothing_non_manifold = 0;
  int do_nothing_not_oriented = 0;
  int do_nothing_border = 0;

  #define EPSILON 1.0e-10f

  float interval_ranges[] = {0.0f, 5.0f, 10.0f, 20.0f, 30.0f, 60.0f, 90.0f, 180.0f};
  int interval_distribution[] = {0,0,0,0,0,0,0,0};

  while(psreader->read_triangle())
  {
    // compute an (unnormalized) normal for the triangle
    VecCcwNormal3fv(tmp, psreader->t_pos_f[0], psreader->t_pos_f[1], psreader->t_pos_f[2]);

    // normalize
    length = VecLength3fv(tmp);
    if (length > EPSILON)
    {
      normal = allocNormal();
      alloced_normals++;
      if (alloced_normals > max_alloced_normals) max_alloced_normals = alloced_normals;
      VecScalarDiv3fv(normal->value, tmp, length);
    }
    else
    {
      normal = 0;
      zero_normal++;
    }

    // set normal or compute crease angles across the three edges
    for (i = 0; i < 3; i++)
    {
      // set normal for entering edges
      if (PS_IS_ENTERING_EDGE(psreader->t_eflag[i]))
      {
        psreader->set_edata((void*)normal, i);
        if (normal) normal->use++;
      }
      else if (PS_IS_LEAVING_EDGE(psreader->t_eflag[i]))
      {
        other_normal = (Normal*)psreader->get_edata(i);
        if (normal && other_normal)
        {
          crease_angle = compute_crease_angle(normal->value, other_normal->value);
          for (j = 0; j < 8; j++)
          {
            if (crease_angle < interval_ranges[j]+EPSILON)
            {
              interval_distribution[j]++;
              break;
            }
          }
        }
        else
        {
          if (normal == 0 && other_normal == 0)
          {
            do_nothing_both_zero++;
          }
          else if (normal == 0)
          {
            do_nothing_zero++;
          }
          else
          {
            do_nothing_other_zero++;
          }
        }
        if (other_normal)
        {
          other_normal->use--;
          if (other_normal->use == 0)
          {
            deallocNormal(other_normal);
            alloced_normals--;
          }
        }
      }
      else
      {
        // do nothing across border edges, non-manifold edges, or not-oriented edges
        if (PS_IS_NON_MANIFOLD_EDGE(psreader->t_eflag[i]))
        {
          do_nothing_non_manifold++;
        }
        else if (PS_IS_NOT_ORIENTED_EDGE(psreader->t_eflag[i]))
        {
          do_nothing_not_oriented++;
        }
        else if (PS_IS_BORDER_EDGE(psreader->t_eflag[i]))
        {
          do_nothing_border++;
        }
        else
        {
          fprintf(stderr,"ERROR: un-handled case for edge with flag %d\n", psreader->t_eflag[i]);
        }
      }
    }
    if (normal && normal->use == 0)
    {
      deallocNormal(normal);
      alloced_normals--;
    }
  }

  fprintf(stderr,"number of vertices: %d\n",psreader->nverts);
  fprintf(stderr,"number of faces: %d\n",psreader->nfaces);

  psreader->close();

  fprintf(stderr,"total number of ignored (near) zero length normals: %d\n", zero_normal);
  fprintf(stderr,"number of skipped crease angle computation attemps due to:\n");
  fprintf(stderr,"  border edges:    %d\n", do_nothing_border);
  fprintf(stderr,"  non-manifold edges:    %d\n", do_nothing_non_manifold);
  fprintf(stderr,"  not-oriented edges:   %d\n", do_nothing_not_oriented);
  fprintf(stderr,"  normal was zero: %d\n", do_nothing_zero);
  fprintf(stderr,"  other normal was zero: %d\n", do_nothing_other_zero);
  fprintf(stderr,"  both normals were zero: %d\n", do_nothing_both_zero);
  fprintf(stderr,"crease angle distribution:\n");
  fprintf(stderr,"about   0.0 degrees\t%d edges\n",interval_distribution[0]);
  for (j = 1; j < 8; j++)
  {
    fprintf(stderr,"below %5.1f degrees\t%d edges\n",interval_ranges[j],interval_distribution[j]);
  }
  fprintf(stderr,"maximum number of concurrently allocated normals: %d\n", max_alloced_normals);

  return 1;
}
