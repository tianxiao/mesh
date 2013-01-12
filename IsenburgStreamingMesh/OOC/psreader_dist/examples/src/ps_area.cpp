/*
===============================================================================

  FILE:  ps_area.cpp
  
  CONTENTS:
  
    A demo program that uses the Processing Sequence API. Its main purpose
    is to provide other programmers an easy-to-follow example of how the
    Processing Sequence API can be used for some mesh processing task.

    This simple program computes the total surface area of a mesh.
  
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

float compute_triangle_area(float* v0, float* v1, float* v2)
{
  float v01[3];
  float v02[3];
  float v01crossv02[3];

  VecSubtract3fv(v01, v1, v0);
  VecSubtract3fv(v02, v2, v0);
  VecCrossProd3fv(v01crossv02, v01, v02);
  return 0.5f*VecLength3fv(v01crossv02);
}

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
    fprintf(stderr,"ps_area <file_name>\n");
    exit(1);
  }

  PSreader* psreader = 0;

  if (strstr(file_name, ".sma") || strstr(file_name, ".obj") || strstr(file_name, ".smf"))
  {
    FILE* sma_fp = fopen(file_name, "r");
    if (sma_fp == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      exit(1);
    }
    SMreader_sma* smreader_sma = new SMreader_sma();
    smreader_sma->open(sma_fp);
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

  float area = 0.0f;
  float total_area = 0.0f;

  while (psreader->read_triangle())
  {
    // compute area of triangle
    area = compute_triangle_area(psreader->t_pos_f[0],psreader->t_pos_f[1],psreader->t_pos_f[2]);
    // add to total
    total_area += area;
  }

  fprintf(stderr,"v_count %d \n",psreader->v_count);
  fprintf(stderr,"f_count %d \n",psreader->f_count);

  fprintf(stderr,"total_area %f\n",total_area);

  psreader->close();

  return 1;
}
