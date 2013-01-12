/*
===============================================================================

  FILE:  sm_diagram.cpp
  
  CONTENTS:
  
    This little tool visualizes the coherency diagram of a (streaming) mesh.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2004  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    12 January 2005 -- added support for stdin/stdout
    09 January 2005 -- added support for SME format
    19 October 2004 -- added support for SMB format
    19 January 2004 -- updated for SIGGRAPH submission
    06 January 2004 -- created after 'onyx' burned his tail in my candle
  
===============================================================================
*/

#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>

#include "vec3fv.h"
#include "vec3iv.h"

#include "smreader_sma.h"
#include "smreader_smb.h"
#include "smreader_smc.h"
#include "smreader_smd.h"
#include "smreader_ply.h"
#include "psreader_oocc.h"
#include "psreader_lowspan.h"
#include "smconverter.h"

#include "smreader_smc_old.h"

#include "hash_map.h"

// MOUSE INTERFACE

int LeftButtonDown=0;
int MiddleButtonDown=0;
int RightButtonDown=0;
int OldX,OldY,NewX,NewY;
float DistX=0;
float DistY=0;
float DistZ=3;

// VISUALIZATION SETTINGS

float boundingBoxScale=1.0f;
float boundingBoxTranslateX = 0.0f;
float boundingBoxTranslateY = 0.0f;
float boundingBoxTranslateZ = 0.0f;

// GLOBAL CONTROL VARIABLES

int WindowW=1024, WindowH=768;
//int WindowW=800, WindowH=600;
//int WindowW=1280, WindowH=960;
int InteractionMode=1;
int AnimationOn=0;
int WorkingOn=0;
int PrintToFileOn=0;

// COLORS

float colours_diffuse[10][4];

// DATA STORAGE FOR STREAM VISUALIZER OUTPUT

char* file_name = "models/bunny/bun_zipper.ply.gz";
bool isma = false;
bool ismb = false;
bool ismc = false;
bool ismc_old = false;
bool ismd = false;
bool isme = false;
SMreader* smreader = 0;

int nverts = 1;
int nfaces = 1;

int EXACTLY_N_TRIANGLES = 50000;
int EVERY_NTH_TRIANGLE = -1;
int TOTAL_TRIANGLES;
int NEXT_TRIANGLE;

int EXACTLY_N_STEPS = 25;
int EVERY_NTH_STEP = -1;
int TOTAL_STEPS;
int NEXT_STEP;

float pointsize = 1.0f;

float linewidth_maxspans = 3.0f;
float linewidth_box = 3.0f;
float linewidth_bar = 2.0f;

float aspect_ratio = 0.5f;

int SHOW_VERTEX_SPAN=1;
int SHOW_TRIANGLE_SPAN=1;
int SHOW_REFERENCES=1;

int SHOW_MAX_SPANS=1;

int SHOW_COLORBAR=1;
int SHOW_COLORBAR_LINES=20;

unsigned char* Framebuffer = 0;
char* PrintFileName = "frame";
int Time=0;

// efficient memory allocation

typedef struct Span
{
  int start;
  int end;
} Span;

// DATA STORAGE FOR STREAM VISUALIZER OUTPUT

int illustrated_triangles_num = 0;
int illustrated_triangles_alloc = 0;
int* illustrated_triangles = 0;

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void SavePPM(char *FileName, unsigned char* Colour, int Width, int Height)
{
  FILE *fp = fopen(FileName, "wb");
  fprintf(fp, "P6\n%d %d\n255\n", Width, Height);
  int NumRowPixels = Width*3;
  for (int i=(Height-1)*Width*3; i>=0; i-=(Width*3))
  {
    fwrite(&(Colour[i]),1,NumRowPixels,fp);
  }
  fclose(fp);
}

void InitColors()
{
  colours_diffuse[0][0] = 0.0f; colours_diffuse[0][1] = 0.0f; colours_diffuse[0][2] = 0.0f; colours_diffuse[0][3] = 1.0f; // black
  colours_diffuse[1][0] = 0.6f; colours_diffuse[1][1] = 0.0f; colours_diffuse[1][2] = 0.0f; colours_diffuse[1][3] = 1.0f; // red
  colours_diffuse[2][0] = 0.0f; colours_diffuse[2][1] = 0.8f; colours_diffuse[2][2] = 0.0f; colours_diffuse[2][3] = 1.0f; // green
  colours_diffuse[3][0] = 0.0f; colours_diffuse[3][1] = 0.0f; colours_diffuse[3][2] = 0.6f; colours_diffuse[3][3] = 1.0f; // blue
  colours_diffuse[4][0] = 0.6f; colours_diffuse[4][1] = 0.6f; colours_diffuse[4][2] = 0.0f; colours_diffuse[4][3] = 1.0f; // yellow
  colours_diffuse[5][0] = 0.6f; colours_diffuse[5][1] = 0.0f; colours_diffuse[5][2] = 0.6f; colours_diffuse[5][3] = 1.0f; // purple
  colours_diffuse[6][0] = 0.0f; colours_diffuse[6][1] = 0.6f; colours_diffuse[6][2] = 0.6f; colours_diffuse[6][3] = 1.0f; // cyan
  colours_diffuse[7][0] = 0.7f; colours_diffuse[7][1] = 0.7f; colours_diffuse[7][2] = 0.7f; colours_diffuse[7][3] = 1.0f; // white
  colours_diffuse[8][0] = 0.2f; colours_diffuse[8][1] = 0.2f; colours_diffuse[8][2] = 0.6f; colours_diffuse[8][3] = 1.0f; // light blue
  colours_diffuse[9][0] = 0.9f; colours_diffuse[9][1] = 0.4f; colours_diffuse[9][2] = 0.7f; colours_diffuse[9][3] = 1.0f; // violett
} 

int vertex_with_max_triangle_span = -1;
Span max_triangle_span;

int triangle_with_max_vertex_span = -1;
Span max_vertex_span;

typedef hash_map<int, Span> my_hash;

static my_hash* triangle_span_hash = 0;

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"sm_diagram mesh.sma\n");
  fprintf(stderr,"sm_diagram -ismc < mesh.smc\n");
  fprintf(stderr,"sm_diagram -win 640 480 \n");
  fprintf(stderr,"sm_diagram -win 1600 1200 mymesh.obj\n");
  fprintf(stderr,"sm_diagram -h\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"in addition to our formats SMA to SMD this tool\n");
  fprintf(stderr,"supports OBJ, SMF, PLY meshes (optionally gzipped).\n");
}

FILE* file = 0;

void vizBegin()
{
  triangle_with_max_vertex_span = -1;
  max_vertex_span.start = 0;
  max_vertex_span.end = 0;
  vertex_with_max_triangle_span = -1;
  max_triangle_span.start = 0;
  max_triangle_span.end = 0;

  if (file_name == 0 && !isma && !ismb && !ismc && !ismc_old && !ismd && !isme)
  {
    fprintf(stderr,"ERROR: no input\n");
    exit(0);
  }

  if (file_name)
  {
    fprintf(stderr,"loading mesh '%s'...\n",file_name);

    if (strstr(file_name, ".sma") || strstr(file_name, ".obj") || strstr(file_name, ".smf"))
    {
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

      if (smreader_sma->nfaces == -1 || smreader_sma->nverts == -1)
      {
        fprintf(stderr,"need additional pass to count verts and faces\n");
        while (smreader_sma->read_element() > SM_EOF);
        smreader_sma->close();
        fclose(file);

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
          fprintf(stderr,"ERROR: cannot open %s a second time\n",file_name);
          exit(1);
        }
        smreader_sma->open(file);
      }
      smreader = smreader_sma;
    }
    else if (strstr(file_name, ".smb"))
    {
      if (strstr(file_name, ".gz"))
      {
        #ifdef _WIN32
        file = fopenGzipped(file_name, "rb");
        #else
        fprintf(stderr,"ERROR: cannot open gzipped file %s\n",file_name);
        exit(1);
        #endif
      }
      else
      {
        file = fopen(file_name, "rb");
      }
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name);
        exit(1);
      }
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file);

      if (smreader_smb->nfaces == -1 || smreader_smb->nverts == -1)
      {
        fprintf(stderr,"need additional pass to count verts and faces\n");
        while (smreader_smb->read_element() > SM_EOF);
        smreader_smb->close();
        fclose(file);

        if (strstr(file_name, ".gz"))
        {
          #ifdef _WIN32
          file = fopenGzipped(file_name, "rb");
          #else
          fprintf(stderr,"ERROR: cannot open gzipped file %s\n",file_name);
          exit(1);
          #endif
        }
        else
        {
          file = fopen(file_name, "rb");
        }
        if (file == 0)
        {
          fprintf(stderr,"ERROR: cannot open %s a second time\n",file_name);
          exit(1);
        }
        smreader_smb->open(file);
      }
      smreader = smreader_smb;
    }
    else if (strstr(file_name, ".smc_old"))
    {
      file = fopen(file_name, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name);
        exit(1);
      }
      SMreader_smc_old* smreader_smc_old = new SMreader_smc_old();
      smreader_smc_old->open(file);

      if (smreader_smc_old->nfaces == -1 || smreader_smc_old->nverts == -1)
      {
        fprintf(stderr,"need additional pass to count verts and faces\n");
        while (smreader_smc_old->read_element() > SM_EOF);
        smreader_smc_old->close();
        fclose(file);

        file = fopen(file_name, "rb");
        if (file == 0)
        {
          fprintf(stderr,"ERROR: cannot open %s a second time\n",file_name);
          exit(1);
        } 
        smreader_smc_old->open(file);
      } 
      smreader = smreader_smc_old;
    }
    else if (strstr(file_name, ".smc") || strstr(file_name, ".sme"))
    {
      file = fopen(file_name, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name);
        exit(1);
      }
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file);

      if (smreader_smc->nfaces == -1 || smreader_smc->nverts == -1)
      {
        fprintf(stderr,"need additional pass to count verts and faces\n");
        while (smreader_smc->read_element() > SM_EOF);
        smreader_smc->close();
        fclose(file);

        file = fopen(file_name, "rb");
        if (file == 0)
        {
          fprintf(stderr,"ERROR: cannot open %s a second time\n",file_name);
          exit(1);
        } 
        smreader_smc->open(file);
      } 
      smreader = smreader_smc;
    }
    else if (strstr(file_name, ".smd"))
    {
      file = fopen(file_name, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name);
        exit(1);
      }
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file);

      if (smreader_smd->nfaces == -1 || smreader_smd->nverts == -1)
      {
        fprintf(stderr,"need additional pass to count verts and faces\n");
        while (smreader_smd->read_element() > SM_EOF);
        smreader_smd->close();
        fclose(file);

        file = fopen(file_name, "rb");
        if (file == 0)
        {
          fprintf(stderr,"ERROR: cannot open %s a second time\n",file_name);
          exit(1);
        } 
        smreader_smd->open(file);
      } 
      smreader = smreader_smd;
    }
    else if (strstr(file_name, "_compressed") || strstr(file_name, "depthfirst") || strstr(file_name, "depth_first"))
    {
      file = fopen(file_name, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name);
        exit(1);
      }
      PSreader_oocc* psreader_oocc = new PSreader_oocc();
      psreader_oocc->open(file);
      SMconverter* smconverter = new SMconverter();
      smconverter->open(psreader_oocc);
      smreader = smconverter;
    }
    else if (strstr(file_name, "_lowspan") || strstr(file_name, "breadthfirst") || strstr(file_name, "breadth_first"))
    {
      file = fopen(file_name, "rb");
      if (file == 0)
      {
        fprintf(stderr,"ERROR: cannot open %s\n",file_name);
        exit(1);
      }
      PSreader_lowspan* psreader_lowspan = new PSreader_lowspan();
      psreader_lowspan->open(file);
      SMconverter* smconverter = new SMconverter();
      smconverter->open(psreader_lowspan);
      smreader = smconverter;
    }
    else if (strstr(file_name, ".ply"))
    {
      if (strstr(file_name, ".gz"))
      {
        #ifdef _WIN32
          file = fopenGzipped(file_name, "rb");
          if (file == 0)
          {
            fprintf(stderr,"ERROR: cannot open %s\n",file_name);
            exit(1);
          }
          SMreader_ply* smreader_ply = new SMreader_ply();
          smreader_ply->open(file);
          smreader = smreader_ply;
        #else
          fprintf(stderr,"ERROR: cannot open gzipped file %s\n",file_name);
          exit(1);
        #endif
      }
      else
      {
        file = fopen(file_name, "rb");
        if (file == 0)
        {
          fprintf(stderr,"ERROR: cannot open %s\n",file_name);
          exit(1);
        }
        SMreader_ply* smreader_ply = new SMreader_ply();
        smreader_ply->open(file,false,true);
        smreader = smreader_ply;
      }
    }
    else
    {
      fprintf(stderr,"ERROR: cannot guess input type from file name %s \n", file_name);
      exit(1);
    }
  }
  else
  {
    fprintf(stderr,"loading mesh from stdin...\n");

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
    if (isma)
    {
      SMreader_sma* smreader_sma = new SMreader_sma();
      smreader_sma->open(file);
      smreader = smreader_sma;
      isma = false;
    }
    else if (ismb)
    {
      SMreader_smb* smreader_smb = new SMreader_smb();
      smreader_smb->open(file);
      smreader = smreader_smb;
      ismb = false;
    }
    else if (ismc || isme)
    {
      SMreader_smc* smreader_smc = new SMreader_smc();
      smreader_smc->open(file);
      smreader = smreader_smc;
      isme = false;
    }
    else if (ismc_old)
    {
      SMreader_smc_old* smreader_smc_old = new SMreader_smc_old();
      smreader_smc_old->open(file);
      smreader = smreader_smc_old;
      ismc = false;
    }
    else if (ismd)
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file);
      smreader = smreader_smd;
      ismd = false;
    }
  }

  if (smreader->nverts == -1 || smreader->nfaces == 0)
  {
    fprintf(stderr,"ERROR: no vertex and triangle count info ... exiting\n");
    exit(1);
  }

  nverts = smreader->nverts;
  nfaces = smreader->nfaces;

  fprintf(stderr,"nverts %d\n",nverts);
  fprintf(stderr,"nfaces %d\n",nfaces);

  EVERY_NTH_STEP = nfaces / EXACTLY_N_STEPS;
  if (EVERY_NTH_STEP == 0)
  {
    EVERY_NTH_STEP = 1;
  }
  TOTAL_STEPS = (nfaces / EVERY_NTH_STEP) + 1;
  NEXT_STEP = EVERY_NTH_STEP;


  EVERY_NTH_TRIANGLE = nfaces / EXACTLY_N_TRIANGLES;
  if (EVERY_NTH_TRIANGLE == 0)
  {
    EVERY_NTH_TRIANGLE = 1;
  }
  TOTAL_TRIANGLES = (nfaces / EVERY_NTH_TRIANGLE) + 1;
  NEXT_TRIANGLE = 0;

  illustrated_triangles_num = 0;

  if (TOTAL_TRIANGLES > illustrated_triangles_alloc)
  {
    illustrated_triangles_alloc = TOTAL_TRIANGLES;
    if (illustrated_triangles)
    {
      delete [] illustrated_triangles;
    }
    illustrated_triangles = new int[illustrated_triangles_alloc*4];
  }

  if (triangle_span_hash)
  {
    triangle_span_hash->clear();
  }
  else
  {
    triangle_span_hash = new my_hash;
  }
}

void vizEnd()
{
  if (smreader)
  {
    smreader->close();
    fclose(file);
    delete smreader;

    smreader = 0;
  }
}

int vizContinue()
{
  int i;
  Span span;
  SMevent element;
  my_hash::iterator hash_element;

  while (element = smreader->read_element())
  {
    if (element == SM_TRIANGLE)
    {
      if (smreader->f_count >= NEXT_TRIANGLE)
      {
        // x-coordinate (triangle axis)
        illustrated_triangles[illustrated_triangles_num*4] = smreader->f_count;
        for (i = 0; i < 3; i++)
        {
          // y-coordinate (vertex axis)
          illustrated_triangles[illustrated_triangles_num*4+1+i] = smreader->t_idx[i];
        }
        illustrated_triangles_num++;
        NEXT_TRIANGLE += EVERY_NTH_TRIANGLE;

        for (i = 0; i < 3; i++)
        {
          hash_element = triangle_span_hash->find(smreader->t_idx[i]);
          if (hash_element == triangle_span_hash->end())
          {
            span.start = smreader->f_count;
            span.end = smreader->f_count;
            triangle_span_hash->insert(my_hash::value_type(smreader->t_idx[i], span));
          }
          else
          {
            (*hash_element).second.end = smreader->f_count;
          }
        }
      }
      else
      {
        for (i = 0; i < 3; i++)
        {
          hash_element = triangle_span_hash->find(smreader->t_idx[i]);
          if (hash_element != triangle_span_hash->end())
          {
            (*hash_element).second.end = smreader->f_count;
          }
        }
      }
    }

    if (smreader->f_count > NEXT_STEP)
    {
      NEXT_STEP += EVERY_NTH_STEP;
      break;
    }
  }

  if (element)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

void myReshape(int w, int h)
{
  glutReshapeWindow(WindowW,WindowH);
}

void myIdle()
{
  if (AnimationOn)
  {
    AnimationOn = vizContinue();
    if (!AnimationOn)
    {
      WorkingOn = 0;
      vizEnd();
    }
    glutPostRedisplay();
  }
}

void myMouseFunc(int button, int state, int x, int y)
{
  NewX=x; NewY=y;
    
  if (button == GLUT_LEFT_BUTTON)
  {
    LeftButtonDown = !state;
    MiddleButtonDown = 0;
    RightButtonDown = 0;
  }
  else if (button == GLUT_RIGHT_BUTTON)
  {
    LeftButtonDown = 0;
    MiddleButtonDown = 0;
    RightButtonDown = !state;
  }
  else if (button == GLUT_MIDDLE_BUTTON)
  {
    LeftButtonDown = 0;
    MiddleButtonDown = !state;
    RightButtonDown = 0;
  }
}

void myMotionFunc(int x, int y)
{
  OldX=NewX; OldY=NewY;
  NewX=x;    NewY=y;
  
  float RelX = (NewX-OldX) / (float)glutGet((GLenum)GLUT_WINDOW_WIDTH);
  float RelY = (NewY-OldY) / (float)glutGet((GLenum)GLUT_WINDOW_HEIGHT);
  if (LeftButtonDown) 
  { 
    if (InteractionMode == 1)
    {
      DistX-=RelX;
      DistY+=RelY;
    }
    else if (InteractionMode == 2)
    {
      DistZ-=RelY*DistZ;
    }
  }
  else if (MiddleButtonDown)
  {
    DistX-=RelX*1.0f;
    DistY+=RelY*1.0f;
  }

  glutPostRedisplay();
}

void MyMenuFunc(int value);


void myKeyboard(unsigned char Key, int x, int y)
{
  switch(Key)
  {
  case 'Q':
  case 'q':
  case 27:
    exit(0);
    break;
  case ' ': // stationary, translate, or zoom
    if (InteractionMode == 2)
    {
      InteractionMode = 1;
    }
    else
    {
      InteractionMode += 1;
    }
    glutPostRedisplay();
    break;
  case '>': //zoom in
    DistZ-=0.1f;
    break;
  case '<': //zoom out
    DistZ+=0.1f;
    break;
  case '-':
    pointsize -= 1.0f;
    if (pointsize < 1.0f) pointsize = 1.0f;
    fprintf(stderr,"pointsize %d\n",(int)pointsize);
    glutPostRedisplay();
    break;
  case '=':
    pointsize += 1.0f;
    fprintf(stderr,"pointsize %d\n",(int)pointsize);
    glutPostRedisplay();
    break;
  case '_':
    linewidth_maxspans -= 1.0f;
    if (linewidth_maxspans < 1.0f) linewidth_maxspans = 1.0f;
    fprintf(stderr,"linewidth_maxspans %d\n",(int)linewidth_maxspans);
    glutPostRedisplay();
    break;
  case '+':
    linewidth_maxspans += 1.0f;
    fprintf(stderr,"linewidth_maxspans %d\n",(int)linewidth_maxspans);
    glutPostRedisplay();
    break;
  case 'B':
  case 'b':
    SHOW_COLORBAR = !SHOW_COLORBAR;
    fprintf(stderr,"SHOW_COLORBAR %d\n",SHOW_COLORBAR);
    glutPostRedisplay();
    break;
  case 'T':
  case 't':
    SHOW_TRIANGLE_SPAN = !SHOW_TRIANGLE_SPAN;
    fprintf(stderr,"SHOW_TRIANGLE_SPAN %d\n",SHOW_TRIANGLE_SPAN);
    glutPostRedisplay();
    break;
  case 'O':
  case 'o':
    AnimationOn = 0;
    break;
  case 'V':
  case 'v':
    SHOW_VERTEX_SPAN = !SHOW_VERTEX_SPAN;
    fprintf(stderr,"SHOW_VERTEX_SPAN %d\n",SHOW_VERTEX_SPAN);
    glutPostRedisplay();
    break;
  case 'M':
  case 'm':
    SHOW_MAX_SPANS = !SHOW_MAX_SPANS;
    fprintf(stderr,"SHOW_MAX_SPANS %d\n",SHOW_MAX_SPANS);
    glutPostRedisplay();
    break;
  case 'R':
  case 'r':
    SHOW_REFERENCES = !SHOW_REFERENCES;
    fprintf(stderr,"SHOW_REFERENCES %d\n",SHOW_REFERENCES);
    glutPostRedisplay();
    break;
  case 'D':
  case 'd':
    PrintToFileOn=1;
    if (Framebuffer) delete [] Framebuffer;
    Framebuffer = new unsigned char[WindowW*WindowH*3];
    fprintf(stderr,"print_to_file %d\n",PrintToFileOn);
    break;
  case 'P':
  case 'p':
      AnimationOn = !AnimationOn;
  case 'S':
  case 's':
    if (WorkingOn == 0)
    {
      vizBegin();
      WorkingOn = vizContinue();
    }
    else
    {
      WorkingOn = vizContinue();
    }
    if (WorkingOn == 0)
    {
      vizEnd();
      AnimationOn = 0;
      PrintToFileOn = 0;
    }
    glutPostRedisplay();
    break;
  case 'K':
  case 'k':
    printf("DistX = %ff; DistY = %ff; DistZ = %ff;\n",DistX,DistY,DistZ);
    break;
  case 'L':
  case 'l':
    if (SHOW_COLORBAR_LINES) SHOW_COLORBAR_LINES = 0;
    else SHOW_COLORBAR_LINES = 20;
    fprintf(stderr,"SHOW_COLORBAR_LINES %d\n",SHOW_COLORBAR_LINES);
    glutPostRedisplay();
    break;
  case '1':
    MyMenuFunc(1);
    break;
  case '2':
    MyMenuFunc(2);
    break;
  case '3':
    MyMenuFunc(3);
    break;
  case '4':
    MyMenuFunc(4);
    break;
  case '5':
    MyMenuFunc(5);
    break;
  case '6':
    MyMenuFunc(6);
    break;
  case '7':
    MyMenuFunc(7);
    break;
  case '8':
    MyMenuFunc(8);
    break;
  case '9':
    MyMenuFunc(9);
    break;
  case '0':
    MyMenuFunc(10);
    break;
  };
}

void MyBunnyFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/bunny/bun_zipper.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/bunny/bunny-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/bunny/bunny-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/bunny/bunny-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/bunny/bunny-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/bunny/bunny-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/bunny/bunny-depth.smc";
  }
  else if (value == 8)
  {
    file_name = "models/bunny/bunny-rendering.smc";
  }
}

void MyHorseFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/horse/horse.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/horse/horse-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/horse/horse-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/horse/horse-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/horse/horse-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/horse/horse-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/horse/horse-depth.smc";
  }
  else if (value == 8)
  {
    file_name = "models/horse/horse-rendering.smc";
  }
}

void MyDinosaurFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/dinosaur/dinosaur.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/dinosaur/dinosaur-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/dinosaur/dinosaur-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/dinosaur/dinosaur-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/dinosaur/dinosaur-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/dinosaur/dinosaur-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/dinosaur/dinosaur-depth.smc";
  }
  else if (value == 8)
  {
    file_name = "models/dinosaur/dinosaur-rendering.smc";
  }
}

void MyArmadilloFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/armadillo/armadillo.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/armadillo/armadillo-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/armadillo/armadillo-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/armadillo/armadillo-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/armadillo/armadillo-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/armadillo/armadillo-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/armadillo/armadillo-depth.smc";
  }
  else if (value == 8)
  {
    file_name = "models/armadillo/armadillo-rendering.smc";
  }
}

void MyDragonFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/dragon/dragon_vrip.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/dragon/dragon-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/dragon/dragon-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/dragon/dragon-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/dragon/dragon-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/dragon/dragon-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/dragon/dragon-depth.smc";
  }
  else if (value == 8)
  {
    file_name = "models/dragon/dragon-rendering.smc";
  }
  else if (value == 9)
  {
    file_name = "models/dragon/dragon-depthfirst.oocc";
  }
  else if (value == 10)
  {
    file_name = "models/dragon/dragon-breadthfirst.oocc";
  }
  else if (value == 11)
  {
    file_name = "models/dragon/dragon-zorder.smc";
  }
}

void MyBuddhaFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/buddha/happy_vrip.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/buddha/buddha-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/buddha/buddha-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/buddha/buddha-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/buddha/buddha-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/buddha/buddha-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/buddha/buddha-depth.smc";
  }
  else if (value == 8)
  {
    file_name = "models/buddha/buddha-rendering.smc";
  }
  else if (value == 9)
  {
    file_name = "models/buddha/buddha-breadthfirst.oocc";
  }
  else if (value == 10)
  {
    file_name = "models/buddha/buddha-depthfirst.oocc";
  }
}

void MyAsianDragonFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/asian-dragon/asian-dragon.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/asian-dragon/asian-dragon-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/asian-dragon/asian-dragon-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/asian-dragon/asian-dragon-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/asian-dragon/asian-dragon-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/asian-dragon/asian-dragon-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/asian-dragon/asian-dragon-depth.smc";
  }
}

void MyThaiStatueFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/thai-statue/thai-statue.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/thai-statue/thai-statue-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/thai-statue/thai-statue-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/thai-statue/thai-statue-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/thai-statue/thai-statue-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/thai-statue/thai-statue-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/thai-statue/thai-statue-depth.smc";
  }
}

void MyLucyFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/lucy/lucy.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/lucy/lucy-interleaved.smb.gz";
  }
  else if (value == 3)
  {
    file_name = "models/lucy/lucy-vcompact.smc";
  }
  else if (value == 4)
  {
    file_name = "models/lucy/lucy-spectral.smc";
  }
  else if (value == 5)
  {
    file_name = "models/lucy/lucy-geometric.smc";
  }
  else if (value == 6)
  {
    file_name = "models/lucy/lucy-breadth.smc";
  }
  else if (value == 7)
  {
    file_name = "models/lucy/lucy-depth.smc";
  }
}

void MyMenuFunc(int value)
{
  if (value >= 100)
  {
    if (value <= 102)
    {
      InteractionMode = value - 100;
      glutPostRedisplay();
    }
    else if (value == 103)
    {
      myKeyboard('s',0,0);
    }
    else if (value == 104)
    {
      myKeyboard('p',0,0);
    }
    else if (value == 105)
    {
      myKeyboard('o',0,0);
    }
    else if (value == 109)
    {
      myKeyboard('q',0,0);
    }
    else if (value == 151)
    {
      myKeyboard('r',0,0);
    }
    else if (value == 152)
    {
      myKeyboard('v',0,0);
    }
    else if (value == 153)
    {
      myKeyboard('t',0,0);
    }
    else if (value == 154)
    {
      myKeyboard('m',0,0);
    }
  }
  else if (value == 1)
  {
    file_name = "models/dragon/dragon-depthfirst.oocc";
  }
  else if (value == 2)
  {
    file_name = "models/dragon/dragon-breadthfirst.oocc";
  }
  else if (value == 3)
  {
    file_name = "models/dragon/dragon-zorder.smc";
  }
  else if (value == 4)
  {
    file_name = "models/dragon/dragon-geometric.smc";
  }
  else if (value == 5)
  {
    file_name = "models/bunny/bun_zipper.ply.gz";
  }
  else if (value == 6)
  {
    file_name = "models/horse/horse.ply.gz";
  }
  else if (value == 7)
  {
    file_name = "models/dinosaur/dinosaur.ply.gz";
  }
  else if (value == 8)
  {
    file_name = "models/armadillo/armadillo.ply.gz";
  }
  else if (value == 9)
  {
    file_name = "models/dragon/dragon_vrip.ply.gz";
  }
  else if (value == 10)
  {
    file_name = "models/buddha/happy_vrip.ply.gz";
  }
  else if (value == 40)
  {
    EXACTLY_N_STEPS = 5;
  }
  else if (value == 41)
  {
    EXACTLY_N_STEPS = 10;
  }
  else if (value == 42)
  {
    EXACTLY_N_STEPS = 25;
  }
  else if (value == 43)
  {
    EXACTLY_N_STEPS = 50;
  }
  else if (value == 44)
  {
    EXACTLY_N_STEPS = 100;
  }
  else if (value == 45)
  {
    EXACTLY_N_STEPS = 250;
  }
  else if (value == 46)
  {
    EXACTLY_N_STEPS = 500;
  }
  else if (value == 47)
  {
    EXACTLY_N_STEPS = 1000;
  }
  else if (value == 61)
  {
    EXACTLY_N_TRIANGLES = 50000;
  }
  else if (value == 62)
  {
    EXACTLY_N_TRIANGLES = 100000;
  }
  else if (value == 63)
  {
    EXACTLY_N_TRIANGLES = 250000;
  }
  else if (value == 64)
  {
    EXACTLY_N_TRIANGLES = 500000;
  }
  else if (value == 65)
  {
    EXACTLY_N_TRIANGLES = 1000000;
  }
  else if (value == 66)
  {
    EXACTLY_N_TRIANGLES = 2000000;
  }
  else if (value == 67)
  {
    EXACTLY_N_TRIANGLES = 3000000;
  }
  else if (value == 68)
  {
    EXACTLY_N_TRIANGLES = 5000000;
  }
  else if (value == 81)
  {
    SHOW_COLORBAR_LINES = 10;
  }
  else if (value == 82)
  {
    SHOW_COLORBAR_LINES = 15;
  }
  else if (value == 83)
  {
    SHOW_COLORBAR_LINES = 20;
  }
  else if (value == 84)
  {
    SHOW_COLORBAR_LINES = 25;
  }
  else if (value == 85)
  {
    SHOW_COLORBAR_LINES = 40;
  }
  else if (value == 86)
  {
    SHOW_COLORBAR_LINES = 50;
  }
  else if (value == 87)
  {
    SHOW_COLORBAR_LINES = 60;
  }
  else if (value == 88)
  {
    SHOW_COLORBAR_LINES = 75;
  }
}

void displayMessage( )
{
  glColor3f( 0.7f, 0.7f, 0.7f );  // Set colour to grey
  glMatrixMode( GL_PROJECTION );
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D( 0.0f, 1.0f, 0.0f, 1.0f );
  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
  glLoadIdentity();
  glRasterPos2f( 0.03f, 0.95f );
  
  if( InteractionMode == 1 )
  {
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'r');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'a');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'n');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 's');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'l');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'a');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'e');
  }
  else
  {
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'z');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'o');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'o');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'm');
  }
  
  glPopMatrix();
  glMatrixMode( GL_PROJECTION );
  glPopMatrix();
}

void myDisplay()
{
  int sample[3];

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT);
  
  glViewport(0,0,WindowW,WindowH);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0f,(float)WindowW/WindowH,0.00125f,5.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(DistX,DistY,DistZ, DistX,DistY,0, 0,1,0);

  glLineWidth(linewidth_box);

  glColor3fv(colours_diffuse[0]);
  glBegin(GL_LINE_LOOP);
    glVertex3f(-1.0f, aspect_ratio, 0.0f);
    glVertex3f(1.0f, aspect_ratio, 0.0f);
    glVertex3f(1.0f, -aspect_ratio, 0.0f);
    glVertex3f(-1.0f, -aspect_ratio, 0.0f);
  glEnd();

  glLineWidth(1.0f);

  glTranslatef(-1.0, aspect_ratio, 0.0f);

  if (nfaces && nverts)
  {
    glScalef(2.0f/nfaces, -(aspect_ratio*2)/nverts, 1.0f);
  }

  // draw vertex spans (grey)

  if (SHOW_VERTEX_SPAN && illustrated_triangles_num)
  {
    // z-coordinate
    sample[2] = 0;

    glColor3fv(colours_diffuse[7]);
    glBegin(GL_LINES);
    for (int i = 0, j = 0; i < illustrated_triangles_num; i++,j+=3)
    {
      // x-coordinate (triangle axis)
      sample[0] = illustrated_triangles[j++];

      if (illustrated_triangles[j+0] < illustrated_triangles[j+1])
      {
        if (illustrated_triangles[j+0] < illustrated_triangles[j+2])
        {
          sample[1] = illustrated_triangles[j+0];
          glVertex3iv(sample);
          if (illustrated_triangles[j+1] > illustrated_triangles[j+2])
          {
            sample[1] = illustrated_triangles[j+1];
          }
          else
          {
            sample[1] = illustrated_triangles[j+2];
          }
          glVertex3iv(sample);

          if (SHOW_MAX_SPANS)
          {
            if ( (sample[1] - illustrated_triangles[j+0]) > (max_vertex_span.end - max_vertex_span.start) )
            {
              triangle_with_max_vertex_span = sample[0];
              max_vertex_span.start = illustrated_triangles[j+0];
              max_vertex_span.end = sample[1];
            }
          }
        }
        else
        {
          sample[1] = illustrated_triangles[j+2];
          glVertex3iv(sample);
          sample[1] = illustrated_triangles[j+1];
          glVertex3iv(sample);

          if (SHOW_MAX_SPANS)
          {
            if ( (sample[1] - illustrated_triangles[j+2]) > (max_vertex_span.end - max_vertex_span.start) )
            {
              triangle_with_max_vertex_span = sample[0];
              max_vertex_span.start = illustrated_triangles[j+2];
              max_vertex_span.end = sample[1];
            }
          }
        }
      }
      else
      {
        if (illustrated_triangles[j+1] < illustrated_triangles[j+2])
        {
          sample[1] = illustrated_triangles[j+1];
          glVertex3iv(sample);
          if (illustrated_triangles[j+0] > illustrated_triangles[j+2])
          {
            sample[1] = illustrated_triangles[j+0];
          }
          else
          {
            sample[1] = illustrated_triangles[j+2];
          }
          glVertex3iv(sample);

          if (SHOW_MAX_SPANS)
          {
            if ( (sample[1] - illustrated_triangles[j+1]) > (max_vertex_span.end - max_vertex_span.start) )
            {
              triangle_with_max_vertex_span = sample[0];
              max_vertex_span.start = illustrated_triangles[j+1];
              max_vertex_span.end = sample[1];
            }
          }
        }
        else
        {
          sample[1] = illustrated_triangles[j+2];
          glVertex3iv(sample);
          sample[1] = illustrated_triangles[j+0];
          glVertex3iv(sample);

          if (SHOW_MAX_SPANS)
          {
            if ( (sample[1] - illustrated_triangles[j+2]) > (max_vertex_span.end - max_vertex_span.start) )
            {
              triangle_with_max_vertex_span = sample[0];
              max_vertex_span.start = illustrated_triangles[j+2];
              max_vertex_span.end = sample[1];
            }
          }
        }
      }
    }
    glEnd();
  }

  // draw triangle spans (green)

  if (SHOW_TRIANGLE_SPAN && triangle_span_hash)
  {
    // z-coordinate
    sample[2] = 0;

    glColor3fv(colours_diffuse[6]);
    glBegin(GL_LINES);
    for (my_hash::iterator hash_element = triangle_span_hash->begin(); hash_element != triangle_span_hash->end(); hash_element++)
    {
      // y-coordinate (vertex axis)
      sample[1] = (*hash_element).first;
      // x-coordinate (triangle axis)
      sample[0] = (*hash_element).second.start;
      // start line
      glVertex3iv(sample);
      // x-coordinate (triangle axis)
      sample[0] = (*hash_element).second.end;
      // end line
      glVertex3iv(sample);

      if (SHOW_MAX_SPANS)
      {
        if ( ((*hash_element).second.end - (*hash_element).second.start) > (max_triangle_span.end - max_triangle_span.start) )
        {
          vertex_with_max_triangle_span = (*hash_element).first;
          max_triangle_span = (*hash_element).second;
        }
      }
    }
    glEnd();
  }

  // draw the cross references as points (violett)

  if (SHOW_REFERENCES && illustrated_triangles_num)
  {
    // set point size
    glPointSize(pointsize);

    // z-coordinate
    sample[2] = 0;

    glColor3fv(colours_diffuse[5]);
    glBegin(GL_POINTS);
    for (int i = 0, j = 0; i < illustrated_triangles_num; i++)
    {
      // x-coordinate (triangle axis)
      sample[0] = illustrated_triangles[j++];
      // y-coordinate (vertex axis)
      sample[1] = illustrated_triangles[j++];
      // draw point
      glVertex3iv(sample);
      // y-coordinate (vertex axis)
      sample[1] = illustrated_triangles[j++];
      // draw point
      glVertex3iv(sample);
      // y-coordinate (vertex axis)
      sample[1] = illustrated_triangles[j++];
      // draw point
      glVertex3iv(sample);
    }
    glEnd();
  }

  // draw the max spans (red)

  if (SHOW_MAX_SPANS && vertex_with_max_triangle_span != -1)
  {
    glLineWidth(linewidth_maxspans);
    glColor3fv(colours_diffuse[1]);
    glBegin(GL_LINES);
    // y-coordinate (vertex axis)
    sample[1] = vertex_with_max_triangle_span;
    // x-coordinate (triangle axis)
    sample[0] = max_triangle_span.start;
    // start line
    glVertex3iv(sample);
    // x-coordinate (triangle axis)
    sample[0] = max_triangle_span.end;
    // end line
    glVertex3iv(sample);
    glEnd();
    glLineWidth(1.0f);
  }

  if (SHOW_MAX_SPANS && triangle_with_max_vertex_span != -1)
  {
    glLineWidth(linewidth_maxspans);
    glColor3fv(colours_diffuse[1]);
    glBegin(GL_LINES);
    // x-coordinate (triangle axis)
    sample[0] = triangle_with_max_vertex_span;
    // y-coordinate (vertex axis)
    sample[1] = max_vertex_span.start;
    // start line
    glVertex3iv(sample);
    // y-coordinate (vertex axis)
    sample[1] = max_vertex_span.end;
    // end line
    glVertex3iv(sample);
    glEnd();
    glLineWidth(1.0f);
  }

  if (SHOW_COLORBAR)
  {
    int i;
    glLineWidth(linewidth_bar);
    glBegin(GL_LINES);
    for (i = 0; i < illustrated_triangles_num; i++)
    {
      if (i < TOTAL_TRIANGLES/3)
      {
        glColor3f(0.1f+0.8f*i/(TOTAL_TRIANGLES/3), 0.1f, 0.1f);
      }
      else if (i < 2*(TOTAL_TRIANGLES/3))
      {
        glColor3f(0.9f, 0.1f+0.8f*(i-(TOTAL_TRIANGLES/3))/(TOTAL_TRIANGLES/3), 0.1f);
      }
      else
      {
        glColor3f(0.9f, 0.9f, 0.1f+0.8f*(i-2*(TOTAL_TRIANGLES/3))/(TOTAL_TRIANGLES/3));
      }
      // x-coordinate (triangle axis)
      sample[0] = i*EVERY_NTH_TRIANGLE;
      // y-coordinate (vertex axis)
      sample[1] = (int)(1.025f*nverts);
      glVertex3iv(sample);
      sample[1] = (int)(1.075f*nverts);
      glVertex3iv(sample);
    }
    glEnd();

    if (SHOW_COLORBAR_LINES)
    {
      glLineWidth(3.0f);
      glColor3f(0.0f, 0.0f, 0.0f);
      glBegin(GL_LINES);
      for (i = 1; i < SHOW_COLORBAR_LINES; i++)
      {
        // x-coordinate (triangle axis)
        sample[0] = i*nfaces/SHOW_COLORBAR_LINES;
        // y-coordinate (vertex axis)
        sample[1] = (int)(1.025f*nverts);
        glVertex3iv(sample);
        sample[1] = (int)(1.075f*nverts);
        glVertex3iv(sample);
      }
      glEnd();
    }
  }
  
  if (!PrintToFileOn)
  {
    displayMessage();
  }
  else
  {
    char FileName[256], Command[256];
    sprintf(FileName, "./temp.ppm");
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    #ifdef _WIN32
    glReadPixels( 0, 0, WindowW, WindowH, GL_RGB, GL_UNSIGNED_BYTE, Framebuffer );
    #else
    glReadPixels( (1024-WindowW)/2, (1024-WindowH)/2, WindowW, WindowH, GL_RGB, GL_UNSIGNED_BYTE, Framebuffer );
    #endif
    SavePPM(FileName, Framebuffer, WindowW, WindowH);
    #ifdef _WIN32
      sprintf(Command, "i_view32.exe temp.ppm /convert=%s%d%d%d%d.jpg",PrintFileName,((Time/1000)%10),((Time/100)%10),((Time/10)%10),(Time%10));
    #else
      sprintf(Command, "convert ./temp.ppm ./%s%d%d%d%d.jpg",PrintFileName,((Time/1000)%10),((Time/100)%10),((Time/10)%10),(Time%10));
//      sprintf(Command, "imgcopy -q%d ./temp.ppm ./pics/%s%d%d%d%d.jpg",PrintQuality,PrintFileName,((Time/1000)%10),((Time/100)%10),((Time/10)%10),(Time%10));
    #endif
    printf("performing: '%s'\n", Command);
    system(Command);
    Time++;
  }
  glutSwapBuffers();
}

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    usage();
  }
  else
  {
    if (strcmp(argv[1],"-h") == 0) 
    {
      usage();
      exit(0);
    }
    else if (strcmp(argv[1],"-help") == 0)
    {
      usage();
      exit(0);
    }
    for (int i = 1; i < argc; i++)
    {
      if (strcmp(argv[i],"-win") == 0)
      {
        i++;
        WindowW = atoi(argv[i]);
        i++;
        WindowH = atoi(argv[i]);
      }
      else if (strcmp(argv[i],"-isma") == 0 || strcmp(argv[i],"-sma") == 0)
      {
        isma = true;
      }
      else if (strcmp(argv[i],"-ismb") == 0 || strcmp(argv[i],"-smb") == 0)
      {
        ismb = true;
      }
      else if (strcmp(argv[i],"-ismc") == 0 || strcmp(argv[i],"-smc") == 0)
      {
        ismc = true;
      }
      else if (strcmp(argv[i],"-ismc_old") == 0 || strcmp(argv[i],"-smc_old") == 0)
      {
        ismc_old = true;
      }
      else if (strcmp(argv[i],"-ismd") == 0 || strcmp(argv[i],"-smd") == 0)
      {
        ismd = true;
      }
      else if (strcmp(argv[i],"-isme") == 0 || strcmp(argv[i],"-sme") == 0)
      {
        isme = true;
      }
      else if (i == argc-1)
      {
        file_name = argv[argc-1];
      }
    }
  }

  if (isma || ismb || ismc || ismd || isme)
  {
    file_name = 0;
  }

  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(WindowW,WindowH);
  glutInitWindowPosition(180,100);
  glutCreateWindow("Mesh Layout Coherency Diagram");
    
  InitColors();
  
  glutDisplayFunc(myDisplay);
  glutReshapeFunc(myReshape);
  glutIdleFunc(myIdle);
  
  glutMouseFunc(myMouseFunc);
  glutMotionFunc(myMotionFunc);
  glutKeyboardFunc(myKeyboard);
    
  // steps sub menu
  int menuSteps = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("in 5 steps", 40);
  glutAddMenuEntry("in 10 steps", 41);
  glutAddMenuEntry("in 25 steps", 42);
  glutAddMenuEntry("in 50 steps", 43);
  glutAddMenuEntry("in 100 steps", 44);
  glutAddMenuEntry("in 250 steps", 45);
  glutAddMenuEntry("in 500 steps", 46);
  glutAddMenuEntry("in 1000 steps", 47);

  // triangle sub menu
  int menuTriangles = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("50,000 triangles", 61);
  glutAddMenuEntry("100,000 triangles", 62);
  glutAddMenuEntry("250,000 triangles", 63);
  glutAddMenuEntry("500,000 triangles", 64);
  glutAddMenuEntry("1,000,000 triangles", 65);
  glutAddMenuEntry("2,000,000 triangles", 66);
  glutAddMenuEntry("3,000,000 triangles", 67);
  glutAddMenuEntry("5,000,000 triangles", 68);

  // color bar lines sub menu
  int menuLines = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("10 lines", 81);
  glutAddMenuEntry("15 lines", 82);
  glutAddMenuEntry("20 lines", 83);
  glutAddMenuEntry("25 lines", 84);
  glutAddMenuEntry("40 lines", 85);
  glutAddMenuEntry("50 lines", 86);
  glutAddMenuEntry("60 lines", 87);
  glutAddMenuEntry("75 lines", 88);

  // reference grid sub menu
  int menuGrid = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("128 x 128", 75);
  glutAddMenuEntry("256 x 256", 76);
  glutAddMenuEntry("512 x 512", 77);
  glutAddMenuEntry("1024 x 1024", 78);
  glutAddMenuEntry("2048 x 2048", 79);
  glutAddMenuEntry("4096 x 4096", 80);
  glutAddMenuEntry("8192 x 8192", 90);
  glutAddMenuEntry("16384 x 16384", 91);

  int menuBunny = glutCreateMenu(MyBunnyFunc);
  glutAddMenuEntry("original PLY", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);
  glutAddMenuEntry("rendering", 8);

  int menuHorse = glutCreateMenu(MyHorseFunc);
  glutAddMenuEntry("original PLY", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);
  glutAddMenuEntry("rendering", 8);

  int menuDinosaur = glutCreateMenu(MyDinosaurFunc);
  glutAddMenuEntry("original PLY", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);
  glutAddMenuEntry("rendering", 8);

  int menuArmadillo = glutCreateMenu(MyArmadilloFunc);
  glutAddMenuEntry("original PLY", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);
  glutAddMenuEntry("rendering", 8);

  int menuDragon = glutCreateMenu(MyDragonFunc);
  glutAddMenuEntry("original PLY", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("zorder", 11);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);
  glutAddMenuEntry("rendering", 8);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("depth-first oocc", 9);
  glutAddMenuEntry("breadth-first oocc", 10);

  int menuBuddha = glutCreateMenu(MyBuddhaFunc);
  glutAddMenuEntry("original PLY", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);
  glutAddMenuEntry("rendering", 8);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("depth-first oocc", 9);
  glutAddMenuEntry("breadth-first oocc", 10);

  int menuAsianDragon = glutCreateMenu(MyAsianDragonFunc);
//  glutAddMenuEntry("original PLY (slow!!!)", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);

  int menuThaiStatue = glutCreateMenu(MyThaiStatueFunc);
//  glutAddMenuEntry("original PLY (slow!!!)", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);

  int menuLucy = glutCreateMenu(MyLucyFunc);
  glutAddMenuEntry("original PLY (slow!!!)", 1);
  glutAddMenuEntry("vcompact", 3);
  glutAddMenuEntry("spectral", 4);
  glutAddMenuEntry("geometric", 5);
  glutAddMenuEntry("breadth", 6);
  glutAddMenuEntry("depth", 7);

  int menuModels = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("dragon (depth-first oocc) <1>", 1);
  glutAddMenuEntry("dragon (breadth-first oocc) <2>", 2);
  glutAddMenuEntry("dragon (zorder) <3>", 3);
  glutAddMenuEntry("dragon (geometric) <4>", 4);
  glutAddMenuEntry("", 0);
  glutAddSubMenu("bunny ...<5>", menuBunny);
  glutAddSubMenu("horse ...<6>", menuHorse);
  glutAddSubMenu("dinosaur ...<7>", menuDinosaur);
  glutAddSubMenu("armadillo ...<8>", menuArmadillo);
  glutAddSubMenu("dragon ...<9>", menuDragon);
  glutAddSubMenu("buddha ...<0>", menuBuddha);
  glutAddSubMenu("asian-dragon ...", menuAsianDragon);
  glutAddSubMenu("thai-statue ...", menuThaiStatue);
  glutAddSubMenu("lucy ...", menuLucy);

  // main menu
  glutCreateMenu(MyMenuFunc);
  glutAddSubMenu("models ...", menuModels);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("translate <SPACE>", 101);
  glutAddMenuEntry("zoom <SPACE>", 102);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<s>tep", 103);
  glutAddMenuEntry("<p>lay", 104);
  glutAddMenuEntry("st<o>p", 105);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("show <r>eferences (+/-)", 151);
  glutAddMenuEntry("show <v>-span", 152);
  glutAddMenuEntry("show <t>-span", 153);
  glutAddMenuEntry("show <m>ax", 154);
  glutAddMenuEntry("", 0);
  glutAddSubMenu("steps ...", menuSteps);
  glutAddSubMenu("sampling ...", menuTriangles);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("color <b>ar", 152);
  glutAddSubMenu("color bar <l>ines ...", menuLines);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<Q>UIT", 109);
  glutAttachMenu(GLUT_RIGHT_BUTTON);

  glutMainLoop();

  return 0;
}
