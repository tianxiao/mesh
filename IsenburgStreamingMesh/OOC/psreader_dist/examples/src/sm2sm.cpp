/*
===============================================================================

  FILE:  sm2sm.cpp
  
  CONTENTS:
  
    This program inputs a streaming mesh and outputs a streaming mesh in the
    specified format.

    Please note that to convert streaming meshes that use explicit finalization
    (e.g. the 'x' command in the SMA format) to the SMB, SMC, or SME format you
    must use the '-compact' flag.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2005  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    02 May 2005 -- added OFF output format for Ioannis and our SCCG paper
    05 April 2005 -- old SMC becomes SMC_OLD and SME becomes the new SMC
    24 January 2005 -- improved SME takes place of old SMC
    12 January 2005 -- added support for binary stdin/stdout under windows
    09 January 2005 -- adapted from sma2smc after a salmon omelett with Henna
    12 August 2004 -- added deletion of smreader and smwriter
    08 January 2004 -- updated to handle non-compact meshes for Ajith
    07 October 2003 -- initial version created the day of the California recall
  
===============================================================================
*/
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "smreader_sma.h"
#include "smreader_smb.h"
#include "smreader_smc.h"
#include "smreader_smd.h"
#include "smreader_ply.h"
#include "smwriter_sma.h"
#include "smwriter_smb.h"
#include "smwriter_smc.h"
#include "smwriter_smd.h"
#include "smwriter_off.h"

// only supported for legacy reasons
#include "smreader_smc_old.h"
#include "smwriter_smc_old.h"

#include "smreadpreascompactpre.h"
#include "smreadpostascompactpre.h"

#include "smwritebuffered.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
extern "C" int gettime_in_msec();
extern "C" int gettime_in_sec();
extern "C" void settime();
#endif

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"sm2sm -i mesh.smb -o mesh.sme \n");
  fprintf(stderr,"sm2sm -i mesh.sma -o mesh.sme -delay 100 -bits 10\n");
  fprintf(stderr,"sm2sm -isma -osme < mesh.sma > mesh.sme\n");
  fprintf(stderr,"sm2sm -compact -i mesh.sma -mesh.smd -dry\n");
  fprintf(stderr,"sm2sm -i mesh.sma.gz -o mesh.smc -b 12\n");
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
  bool ismc_old = 0;
  bool ismd = 0;
  bool isme = 0;
  bool osma = 0;
  bool osmb = 0;
  bool osmc = 0;
  bool osmc_old = 0;
  bool osmd = 0;
  bool osme = 0;
  bool ooff = 0;
  int bits = 16;
  bool delay = false;
  int delay_value = 0;
  bool compact = 0;
  char* file_name_in = 0;
  char* file_name_out = 0;

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-dry") == 0)
    {
      dry = true;
    }
    else if (strcmp(argv[i],"-isma") == 0)
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
    else if (strcmp(argv[i],"-ismc_old") == 0)
    {
      ismc_old = true;
    }
    else if (strcmp(argv[i],"-ismd") == 0)
    {
      ismd = true;
    }
    else if (strcmp(argv[i],"-isme") == 0)
    {
      isme = true;
    }
    else if (strcmp(argv[i],"-osma") == 0)
    {
      osma = true;
    }
    else if (strcmp(argv[i],"-osmb") == 0)
    {
      osmb = true;
    }
    else if (strcmp(argv[i],"-osmc") == 0)
    {
      osmc = true;
    }
    else if (strcmp(argv[i],"-osmc_old") == 0)
    {
      osmc_old = true;
    }
    else if (strcmp(argv[i],"-osmd") == 0)
    {
      osmd = true;
    }
    else if (strcmp(argv[i],"-osme") == 0)
    {
      osme = true;
    }
    else if (strcmp(argv[i],"-ooff") == 0)
    {
      ooff = true;
    }
    else if (strcmp(argv[i],"-b") == 0 || strcmp(argv[i],"-bits") == 0)
    {
      i++;
      bits = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-delay") == 0)
    {
      delay = true;
      if (i+1 < argc)
      {
        delay_value = atoi(argv[i+1]);
        if (delay_value)
        {
          i++;
        }
      }
    }
    else if (strcmp(argv[i],"-compact") == 0)
    {
      compact = true;
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
    else
    {
      usage();
    }
  }

  SMreader* smreader;
  FILE* file_in;
  
  if (file_name_in)
  {
    if (strstr(file_name_in, ".gz"))
    {
#ifdef _WIN32
      if (strstr(file_name_in, ".sma.gz"))
      {
        file_in = fopenGzipped(file_name_in, "r");
      }
      else if (strstr(file_name_in, ".smb.gz") || strstr(file_name_in, ".smc.gz") || strstr(file_name_in, ".smd.gz") || strstr(file_name_in, ".sme.gz") || strstr(file_name_in, ".ply.gz"))
      {
        file_in = fopenGzipped(file_name_in, "rb");
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma.gz .smb.gz .smc.gz or .smd.gz\n",file_name_in);
        exit(0);
      }
#else
      fprintf(stderr,"ERROR: cannot open gzipped file '%s'\n",file_name_in);
      exit(1);
#endif
    }
    else
    {
      if (strstr(file_name_in, ".sma"))
      {
        file_in = fopen(file_name_in, "r");
        fprintf(stderr,"opening '%s'\n",file_name_in);
      }
      else if (strstr(file_name_in, ".smb") || strstr(file_name_in, ".smc") || strstr(file_name_in, ".smd") || strstr(file_name_in, ".sme"))
      {
        file_in = fopen(file_name_in, "rb");
        fprintf(stderr,"opening '%s'\n",file_name_in);
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma .smb .smc or .smd\n",file_name_in);
        exit(0);
      }
    }
  }
  else
  {
    if (isma || ismb || ismc || ismc_old || ismd || isme)
    {
      file_in = stdin;
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

  if (file_in == 0)
  {
    fprintf(stderr,"ERROR: cannot open '%s' for read\n", file_name_in);
    exit(0);
  }
  
  if (file_name_in)
  {
    if (strstr(file_name_in, ".sma"))
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file_in);
      smreader = smreader_sma;
    }
    else if (strstr(file_name_in, ".smb"))
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file_in);
      smreader = smreader_smb;
    }
    else if (strstr(file_name_in, ".smc_old"))
    {
      SMreader_smc_old* smreader_smc_old = new SMreader_smc_old();
      smreader_smc_old->open(file_in);
      smreader = smreader_smc_old;
    }
    else if (strstr(file_name_in, ".smd"))
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file_in);
      smreader = smreader_smd;
    }
    else if (strstr(file_name_in, ".smc") || strstr(file_name_in, ".sme"))
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file_in);
      smreader = smreader_smc;
    }
    else if (strstr(file_name_in, ".ply"))
    {
      SMreader_ply* smreader_ply = new SMreader_ply();
      smreader_ply->open(file_in);
      smreader = smreader_ply;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine which reader to use for '%s'\n", file_name_in);
      exit(0);
    }
  }
  else
  {
    if (isma)
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file_in);
      smreader = smreader_sma;
    }
    else if (ismb)
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file_in);
      smreader = smreader_smb;
    }
    else if (ismc_old)
    {
      SMreader_smc_old* smreader_smc_old = new SMreader_smc_old();
      smreader_smc_old->open(file_in);
      smreader = smreader_smc_old;
    }
    else if (ismd)
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file_in);
      smreader = smreader_smd;
    }
    else if (ismc || isme)
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file_in);
      smreader = smreader_smc;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine which reader to use\n");
      exit(0);
    }
  }

  if (smreader->post_order)
  {
    fprintf(stderr,"WARNING: applying filter PostAsCompactPre...\n");
    SMreadPostAsCompactPre* smreadpostascompactpre = new SMreadPostAsCompactPre();
    smreadpostascompactpre->open(smreader);
    smreader = smreadpostascompactpre;
  }
  else if (compact)
  {
    fprintf(stderr,"WARNING: applying filter PreAsCompactPre...\n");
    SMreadPreAsCompactPre* smreadpreascompactpre = new SMreadPreAsCompactPre();
    smreadpreascompactpre->open(smreader);
    smreader = smreadpreascompactpre;
  }

  SMwriter* smwriter;
  FILE* file_out;

  if (file_name_out || osma || osmb || osmc || osmc_old || osmd || osme || ooff)
  {
    if (dry)
    {
      fprintf(stderr,"dry write pass. no output.\n");
      file_out = 0;
    }
    else
    {
      if (file_name_out)
      {
        if (strstr(file_name_out, ".sma") || strstr(file_name_out, ".off"))
        {
          file_out = fopen(file_name_out, "w");
        }
        else if (strstr(file_name_out, ".smb") || strstr(file_name_out, ".smc") || strstr(file_name_out, ".smd") || strstr(file_name_out, ".sme"))
        {
          file_out = fopen(file_name_out, "wb");
        }
        else
        {
          fprintf(stderr,"ERROR: output file name '%s' does not end in .sma .smb .smc or .smd\n",file_name_out);
          exit(0);
        }
        if (file_out == 0)
        {
          fprintf(stderr,"ERROR: cannot open '%s' for write\n", file_name_out);
          exit(1);
        }
      }
      else
      {
        file_out = stdout;
#ifdef _WIN32
        if (osma || ooff)
        {
          if(_setmode( _fileno( stdout ), _O_TEXT ) == -1 )
          {
            fprintf(stderr, "ERROR: cannot set stdout to text (translated) mode\n");
          }
        }
        else
        {
          if(_setmode( _fileno( stdout ), _O_BINARY ) == -1 )
          {
            fprintf(stderr, "ERROR: cannot set stdout to binary (untranslated) mode\n");
          }
        }
#endif
      }
    }

    if (file_name_out)
    {
      if (strstr(file_name_out, ".sma"))
      {
        if (file_out)
        {
          SMwriter_sma* smwriter_sma = new SMwriter_sma();
          smwriter_sma->open(file_out);
          smwriter = smwriter_sma;
        }
        else
        {
          smwriter = 0;
        }
      }
      else if (strstr(file_name_out, ".smb"))
      {
        if (file_out)
        {
          SMwriter_smb* smwriter_smb = new SMwriter_smb();
          smwriter_smb->open(file_out);
          smwriter = smwriter_smb;
        }
        else
        {
          smwriter = 0;
        }
      }
      else if (strstr(file_name_out, ".smc") || strstr(file_name_out, ".sme"))
      {
        SMwriter_smc* smwriter_smc = new SMwriter_smc();
        smwriter_smc->open(file_out, bits);
        if (delay)
        {
          SMwriteBuffered* smwrite_buffered = new SMwriteBuffered();
          if (delay_value)
          {
            smwrite_buffered->open(smwriter_smc, delay_value);
          }
          else
          {
            smwrite_buffered->open(smwriter_smc);
          }
          smwriter = smwrite_buffered;
        }
        else
        {
          smwriter = smwriter_smc;
        }
      }
      else if (strstr(file_name_out, ".smd"))
      {
        SMwriter_smd* smwriter_smd = new SMwriter_smd();
        if (delay_value)
        {
          smwriter_smd->open(file_out, bits, delay_value);
        }
        else
        {
          smwriter_smd->open(file_out, bits);
        }
        smwriter = smwriter_smd;
      }
      else if (strstr(file_name_out, ".smc_old"))
      {
        SMwriter_smc_old* smwriter_smc_old = new SMwriter_smc_old();
        smwriter_smc_old->open(file_out, bits);
        if (delay)
        {
          SMwriteBuffered* smwrite_buffered = new SMwriteBuffered();
          if (delay_value)
          {
            smwrite_buffered->open(smwriter_smc_old, delay_value);
          }
          else
          {
            smwrite_buffered->open(smwriter_smc_old);
          }
          smwriter = smwrite_buffered;
        }
        else
        {
          smwriter = smwriter_smc_old;
        }
      }
      else if (strstr(file_name_out, ".off"))
      {
        SMwriter_off* smwriter_off = new SMwriter_off();
        smwriter_off->open(file_out);
        smwriter = smwriter_off;
      }
      else
      {
        fprintf(stderr,"ERROR: cannot determine which writer to use for '%s'\n", file_name_out);
        exit(0);
      }
    }
    else
    {
      if (osma)
      {
        if (file_out)
        {
          SMwriter_sma* smwriter_sma = new SMwriter_sma();
          smwriter_sma->open(file_out);
          smwriter = smwriter_sma;
        }
        else
        {
          smwriter = 0;
        }
      }
      else if (osmb)
      {
        if (file_out)
        {
          SMwriter_smb* smwriter_smb = new SMwriter_smb();
          smwriter_smb->open(file_out);
          smwriter = smwriter_smb;
        }
        else
        {
          smwriter = 0;
        }
      }
      else if (osmc_old)
      {
        SMwriter_smc_old* smwriter_smc_old = new SMwriter_smc_old();
        smwriter_smc_old->open(file_out, bits);
        if (delay)
        {
          SMwriteBuffered* smwrite_buffered = new SMwriteBuffered();
          if (delay_value)
          {
            smwrite_buffered->open(smwriter_smc_old, delay_value);
          }
          else
          {
            smwrite_buffered->open(smwriter_smc_old);
          }
          smwriter = smwrite_buffered;
        }
        else
        {
          smwriter = smwriter_smc_old;
        }
      }
      else if (osmd)
      {
        SMwriter_smd* smwriter_smd = new SMwriter_smd();
        if (delay_value)
        {
          smwriter_smd->open(file_out, bits, delay_value);
        }
        else
        {
          smwriter_smd->open(file_out, bits);
        }
        smwriter = smwriter_smd;
      }
      else if (osmc || osme)
      {
        SMwriter_smc* smwriter_smc = new SMwriter_smc();
        smwriter_smc->open(file_out, bits);
        if (delay)
        {
          SMwriteBuffered* smwrite_buffered = new SMwriteBuffered();
          if (delay_value)
          {
            smwrite_buffered->open(smwriter_smc, delay_value);
          }
          else
          {
            smwrite_buffered->open(smwriter_smc);
          }
          smwriter = smwrite_buffered;
        }
        else
        {
          smwriter = smwriter_smc;
        }
      }
      else if (ooff)
      {
        SMwriter_off* smwriter_off = new SMwriter_off();
        smwriter_off->open(file_out);
        smwriter = smwriter_off;
      }
      else
      {
        fprintf(stderr,"ERROR: cannot determine which writer to use\n");
        exit(0);
      }
    }
  }
  else
  {
    fprintf(stderr,"read pass only.\n");
    smwriter = 0;
  }

  int event;

#ifdef _WIN32
  settime();
#endif

  if (smwriter)
  {
    if (smreader->nverts != -1) smwriter->set_nverts(smreader->nverts);
    if (smreader->nfaces != -1) smwriter->set_nfaces(smreader->nfaces);
    if (smreader->bb_min_f || smreader->bb_max_f) smwriter->set_boundingbox(smreader->bb_min_f, smreader->bb_max_f);

    while (event = smreader->read_element())
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
    fprintf(stderr,"v_count %d %d\n",smreader->v_count,smwriter->v_count);
    fprintf(stderr,"f_count %d %d\n",smreader->f_count,smwriter->f_count);

    smwriter->close();
    if (file_out && file_name_out) fclose(file_out);
    delete smwriter;
  }
  else
  {
    while (event = smreader->read_element());

    fprintf(stderr,"v_count %d\n",smreader->v_count);
    fprintf(stderr,"f_count %d\n",smreader->f_count);
  }

#ifdef _WIN32
  fprintf(stderr,"needed %6.3f seconds\n",0.001f*gettime_in_msec());
#endif

  smreader->close();
  if (file_in && file_name_in) fclose(file_in);
  delete smreader;

  return 1;
}
