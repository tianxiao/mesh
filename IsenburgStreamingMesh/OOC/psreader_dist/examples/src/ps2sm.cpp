/*
===============================================================================

  FILE:  ps2sm.cpp
  
  CONTENTS:
  
    A demo program that uses the Processing Sequence API. Its main purpose
    is to provide other programmers an easy-to-follow example of how the
    Processing Sequence API can be used for some mesh processing task.

    This simple program computes loads in a processing sequence and writes
    it in form of a streaming mesh in ASCII (sma), binary (smb), or compressed
    (smc) format to disk. That streaming mesh is compact and pre-order.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    02 October 2003 -- initial version created on the Thursday that Germany
                       beat Russia 7:1 in the Women Soccer Worlcup
  
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
#include "smconverter.h"
#include "smwriter_sma.h"
#include "smwriter_smb.h"
#include "smwriter_smc.h"

#include "vec3iv.h"
#include "vec3fv.h"

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"ps2sm -i mesh_compressed.ply > mesh.sma\n");
  fprintf(stderr,"ps2sm -i mesh_compressed.smc -dry\n");
  fprintf(stderr,"ps2sm -i mesh.obj -dry\n");
  fprintf(stderr,"ps2sm -i mesh.obj -o mesh.sma\n");
  fprintf(stderr,"ps2sm -b 12 -i mesh.obj -o mesh.smc\n");
  fprintf(stderr,"ps2sm -o mesh.smc < mesh.sma\n");
  fprintf(stderr,"ps2sm -h\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  int i;
  int dry = 0;
  int bits = 16;
  char* file_name_in = 0;
  char* file_name_out = 0;

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-b") == 0)
    {
      i++;
      bits = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name_in = argv[i];
    }
    else if (strcmp(argv[i],"-o") == 0)
    {
      i++;
      file_name_out = argv[i];
    }
    else if (strcmp(argv[i],"-dry") == 0)
    {
      dry = 1;
    }
    else
    {
      usage();
    }
  }

  PSreader* psreader;

  if (file_name_in)
  {
    if (strstr(file_name_in, "compressed") || strstr(file_name_in, "depthfirst"))
    {
      FILE* file = fopen(file_name_in, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name_in);
        exit(1);
      }
      PSreader_oocc* psreader_oocc = new PSreader_oocc();
      if (psreader_oocc->open(file) == 0)
      {
        fprintf(stderr,"ERROR: something went wrong when opening psreader_oocc\n");
        exit(1);
      }
      psreader = psreader_oocc;
    }
    else if (strstr(file_name_in, "lowspan") || strstr(file_name_in, "breadthfirst"))
    {
      FILE* file = fopen(file_name_in, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name_in);
        exit(1);
      }
      PSreader_lowspan* psreader_lowspan = new PSreader_lowspan();
      if (psreader_lowspan->open(file) == 0)
      {
        fprintf(stderr,"ERROR: something went wrong when opening psreader_lowspan\n");
        exit(1);
      }
      psreader = psreader_lowspan;
    }
    else if (strstr(file_name_in, ".sma") || strstr(file_name_in, ".obj") || strstr(file_name_in, ".smf"))
    {
      FILE* file = fopen(file_name_in, "r");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name_in);
        exit(1);
      }
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file);
      PSconverter* psconverter = new PSconverter();
      psconverter->open(smreader_sma, 256, 512);
      psreader = psconverter;
    }
    else if (strstr(file_name_in, ".smb"))
    {
      FILE* file = fopen(file_name_in, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name_in);
        exit(1);
      }
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file);
      PSconverter* psconverter = new PSconverter();
      psconverter->open(smreader_smb, 256, 512);
      psreader = psconverter;
    }
    else if (strstr(file_name_in, ".smc") || strstr(file_name_in, ".sme"))
    {
      FILE* file = fopen(file_name_in, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name_in);
        exit(1);
      }
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file);
      PSconverter* psconverter = new PSconverter();
      psconverter->open(smreader_smc, 256, 512);
      psreader = psconverter;
    }
    else
    {
      fprintf(stderr,"ERROR: input filename must end in *_compressed.ply, *.sma, *.obj, *.smf, *.smb, or *.smc\n");
      exit(1);
    }
  }
  else // assuming SMA stdin input
  {
    SMreader_sma* smreader_sma = new SMreader_sma();
    if (smreader_sma->open(stdin) == 0)
    {
      fprintf(stderr,"ERROR: something went wrong when opening smreader_sma from stdin\n");
    }
    PSconverter* psconverter = new PSconverter();
    psconverter->open(smreader_sma, 256, 512);
    psreader = psconverter;
  }

  SMwriter* smwriter;

  if (file_name_out)
  {
    if (strstr(file_name_out, ".sma"))
    {
      FILE* file = fopen(file_name_out, "w");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open '%s' for write\n",file_name_out);
        exit(1);
      }
      SMwriter_sma* smwriter_sma = new SMwriter_sma();
      if (smwriter_sma->open(file) == 0)
      {
        fprintf(stderr,"ERROR: something went wrong when opening smwriter_sma\n");
        exit(1);
      }
      smwriter = smwriter_sma;
    }
    else if (strstr(file_name_out, ".smc") || strstr(file_name_out, ".sme"))
    {
      FILE* file = fopen(file_name_out, "wb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open '%s' for write\n",file_name_out);
        exit(1);
      }
      SMwriter_smc* smwriter_smc = new SMwriter_smc();
      if (smwriter_smc->open(file, bits) == 0)
      {
        fprintf(stderr,"ERROR: something went wrong when opening smwriter_smc\n");
        exit(1);
      }
      smwriter = smwriter_smc;
    }
    else if (strstr(file_name_out, ".smb"))
    {
      FILE* file = fopen(file_name_out, "wb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open '%s' for write\n",file_name_out);
        exit(1);
      }
      SMwriter_smb* smwriter_smb = new SMwriter_smb();
      if (smwriter_smb->open(file) == 0)
      {
        fprintf(stderr,"ERROR: something went wrong when opening smwriter_smb\n");
        exit(1);
      }
      smwriter = smwriter_smb;
    }
    else
    {
      fprintf(stderr,"ERROR: output filename must end in *.sma or *.smb or *.smc\n");
      exit(1);
    }
  }
  else // assuming SMA stdout output
  {
    SMwriter_sma* smwriter_sma = new SMwriter_sma();
    if (smwriter_sma->open(stdout) == 0)
    {
      fprintf(stderr,"ERROR: something went wrong when opening smwriter_sma to stdout\n");
      exit(1);
    }
    smwriter = smwriter_sma;
  }

  SMconverter* smconverter = new SMconverter();

  if (smconverter->open(psreader) == 0)
  {
    fprintf(stderr,"error:\n");
    fprintf(stderr,"something went wrong when opening smconverter\n");
    exit(1);
  }

  SMreader* smreader = smconverter;

  if (smreader->ncomments != -1)
  {
    for (i = 0; i < smreader->ncomments; i++)
    {
      smwriter->add_comment(smreader->comments[i]);
    }
  }
  if (smreader->nverts != -1) smwriter->set_nverts(smreader->nverts);
  if (smreader->nfaces != -1) smwriter->set_nfaces(smreader->nfaces);
  if ((smreader->bb_min_f != 0) && (smreader->bb_max_f != 0)) smwriter->set_boundingbox(smreader->bb_min_f, smreader->bb_max_f);

  int event;

  while (event = smreader->read_element())
  {
    if (!dry)
    {
      switch (event)
      {
      case SM_VERTEX:
        smwriter->write_vertex(smreader->v_pos_f);
        break;
      case SM_TRIANGLE:
        smwriter->write_triangle(smreader->t_idx, smreader->t_final);
        break;
      case SM_FINALIZED:
        smwriter->write_finalized(smreader->final_idx);
        break;
      default:
        break;
      }
    }
  }

  smwriter->close();
  smreader->close();

  return 1;
}
