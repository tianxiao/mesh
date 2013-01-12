/*
===============================================================================

  FILE:  sm_test.cpp
  
  CONTENTS:
  
    This program is used for testing stuff.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2005  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    25 May 2005 -- changed to test Debug versus Release bug
  
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

#include "vec3fv.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
extern "C" int gettime_in_msec();
extern "C" int gettime_in_sec();
extern "C" void settime();
#endif

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"sm_info mesh.smb \n");
  fprintf(stderr,"sm2sm -h\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  char* file_name1 = 0;
  char* file_name2 = 0;

  if (argc == 3)
  {
    file_name1 = argv[argc-2];
    file_name2 = argv[argc-1];
  }
  else
  {
    usage();
  }

  SMreader* smreader1 = 0;
  FILE* file1 = 0;
  
  if (file_name1)
  {
    if (strstr(file_name1, ".gz"))
    {
#ifdef _WIN32
      if (strstr(file_name1, ".sma.gz"))
      {
        file1 = fopenGzipped(file_name1, "r");
      }
      else if (strstr(file_name1, ".smb.gz") || strstr(file_name1, ".smc.gz") || strstr(file_name1, ".smd.gz") || strstr(file_name1, ".sme.gz") || strstr(file_name1, ".ply.gz"))
      {
        file1 = fopenGzipped(file_name1, "rb");
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma.gz .smb.gz .smc.gz or .smd.gz\n",file_name1);
        exit(0);
      }
#else
      fprintf(stderr,"ERROR: cannot open gzipped file '%s'\n",file_name1);
      exit(1);
#endif
    }
    else
    {
      if (strstr(file_name1, ".sma"))
      {
        file1 = fopen(file_name1, "r");
        fprintf(stderr,"opening '%s'\n",file_name1);
      }
      else if (strstr(file_name1, ".smb") || strstr(file_name1, ".smc") || strstr(file_name1, ".smd") || strstr(file_name1, ".sme"))
      {
        file1 = fopen(file_name1, "rb");
        fprintf(stderr,"opening '%s'\n",file_name1);
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma .smb .smc or .smd\n",file_name1);
        exit(0);
      }
    }
  }

  if (file1 == 0)
  {
    fprintf(stderr,"ERROR: cannot open '%s' for read\n", file_name1);
    exit(0);
  }
  
  if (file_name1)
  {
    if (strstr(file_name1, ".sma"))
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file1);
      smreader1 = smreader_sma;
    }
    else if (strstr(file_name1, ".smb"))
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file1);
      smreader1 = smreader_smb;
    }
    else if (strstr(file_name1, ".smd"))
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file1);
      smreader1 = smreader_smd;
    }
    else if (strstr(file_name1, ".smc") || strstr(file_name1, ".sme"))
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file1);
      smreader1 = smreader_smc;
    }
    else if (strstr(file_name1, ".ply"))
    {
      SMreader_ply* smreader_ply = new SMreader_ply();
      smreader_ply->open(file1);
      smreader1 = smreader_ply;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine which reader to use for '%s'\n", file_name1);
      exit(0);
    }
  }

  SMreader* smreader2 = 0;
  FILE* file2 = 0;
  
  if (file_name2)
  {
    if (strstr(file_name2, ".gz"))
    {
#ifdef _WIN32
      if (strstr(file_name2, ".sma.gz"))
      {
        file2 = fopenGzipped(file_name2, "r");
      }
      else if (strstr(file_name2, ".smb.gz") || strstr(file_name2, ".smc.gz") || strstr(file_name2, ".smd.gz") || strstr(file_name2, ".sme.gz") || strstr(file_name2, ".ply.gz"))
      {
        file2 = fopenGzipped(file_name2, "rb");
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma.gz .smb.gz .smc.gz or .smd.gz\n",file_name2);
        exit(0);
      }
#else
      fprintf(stderr,"ERROR: cannot open gzipped file '%s'\n",file_name2);
      exit(1);
#endif
    }
    else
    {
      if (strstr(file_name2, ".sma"))
      {
        file2 = fopen(file_name2, "r");
        fprintf(stderr,"opening '%s'\n",file_name2);
      }
      else if (strstr(file_name2, ".smb") || strstr(file_name2, ".smc") || strstr(file_name2, ".smd") || strstr(file_name2, ".sme"))
      {
        file2 = fopen(file_name2, "rb");
        fprintf(stderr,"opening '%s'\n",file_name2);
      }
      else
      {
        fprintf(stderr,"ERROR: input file '%s' name does not end in .sma .smb .smc or .smd\n",file_name2);
        exit(0);
      }
    }
  }

  if (file2 == 0)
  {
    fprintf(stderr,"ERROR: cannot open '%s' for read\n", file_name2);
    exit(0);
  }

  if (file_name2)
  {
    if (strstr(file_name2, ".sma"))
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file2);
      smreader2 = smreader_sma;
    }
    else if (strstr(file_name2, ".smb"))
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file2);
      smreader2 = smreader_smb;
    }
    else if (strstr(file_name2, ".smd"))
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file2);
      smreader2 = smreader_smd;
    }
    else if (strstr(file_name2, ".smc") || strstr(file_name2, ".sme"))
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file2);
      smreader2 = smreader_smc;
    }
    else if (strstr(file_name2, ".ply"))
    {
      SMreader_ply* smreader_ply = new SMreader_ply();
      smreader_ply->open(file2);
      smreader2 = smreader_ply;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine which reader to use for '%s'\n", file_name2);
      exit(0);
    }
  }

#ifdef _WIN32
  settime();
#endif

  int event1 = 1;
  int event2 = 1;

  int resolve = 0;

  float v_pos_f1[9];
  float v_pos_f2[9];

  while (event1 && event2)
  {
    event1 = smreader1->read_element();
    event2 = smreader2->read_element();

    if (event1 != event2)
    {
      fprintf(stderr,"event1 %d event2 %d\n",event1,event2);
      fprintf(stderr,"v_count %d %d\n",smreader1->v_count, smreader2->v_count);
      fprintf(stderr,"f_count %d %d\n",smreader1->f_count, smreader2->f_count);
      exit(1);
    }

    switch (event1)
    {
    case SM_VERTEX:
      if (VecEqual3fv(smreader1->v_pos_f,smreader2->v_pos_f) == 0)
      {
        fprintf(stderr,"%f %f %f %f %f %f \n",smreader1->v_pos_f[0],smreader2->v_pos_f[0],smreader1->v_pos_f[1],smreader2->v_pos_f[1],smreader1->v_pos_f[2],smreader2->v_pos_f[2]);
        int* p1 = (int*)(smreader1->v_pos_f);
        int* p2 = (int*)(smreader2->v_pos_f);
        fprintf(stderr,"%d %d %d %d %d %d \n",p1[0],p2[0],p1[1],p2[1],p1[2],p2[2]);
        fprintf(stderr,"v_count %d %d\n",smreader1->v_count, smreader2->v_count);
        fprintf(stderr,"f_count %d %d\n",smreader1->f_count, smreader2->f_count);

        VecCopy3fv(&(v_pos_f1[resolve*3]), smreader1->v_pos_f);
        VecCopy3fv(&(v_pos_f2[resolve*3]), smreader2->v_pos_f);
        resolve++;
      }
      break;
    case SM_TRIANGLE:
      if (resolve)
      {
        if (resolve == 1)
        {
          fprintf(stderr,"not resolved 1\n");
          exit(1);
        }
        else if (resolve == 2)
        {
          if ((VecEqual3fv(&(v_pos_f1[0]),&(v_pos_f2[3])) == 0) || (VecEqual3fv(&(v_pos_f1[3]),&(v_pos_f2[0])) == 0))
          {
            fprintf(stderr,"not resolved 2\n");
            exit(1);
          }
          else
          {
            fprintf(stderr,"resolved 2\n");
          }
        }
        else
        {
          fprintf(stderr,"not resolved 3\n");
          exit(1);
        }
        resolve = 0;
      }
      break;
    case SM_FINALIZED:
      break;
    default:
      break;
    }
  }

  fprintf(stderr,"v_count %d %d\n",smreader1->v_count, smreader2->v_count);
  fprintf(stderr,"f_count %d %d\n",smreader1->f_count, smreader2->f_count);

#ifdef _WIN32
  fprintf(stderr,"needed %6.3f seconds\n",0.001f*gettime_in_msec());
#endif

  smreader1->close();
  smreader2->close();

  if (file1 && file_name1) fclose(file1);
  if (file2 && file_name2) fclose(file2);

  delete smreader1;
  delete smreader2;

  return 1;
}
