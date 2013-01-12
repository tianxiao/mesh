/*
===============================================================================

  FILE:  sm_viewer.cpp
  
  CONTENTS:
  
    This little tool visualizes a streaming mesh (and also not so streaming ones).
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    24 January 2005 -- improved SME takes place of old SMC
    20 January 2005 -- added out-of-core rendering functionality ('r')
    12 January 2005 -- added support for stdin/stdout
    09 January 2005 -- renamed from 'sm_viz.cpp', added support for SME
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

#include "smreadpostascompactpre.h"

#include "positionquantizer.h"

#include "hash_map.h"

// MOUSE INTERFACE

int LeftButtonDown=0;
int MiddleButtonDown=0;
int RightButtonDown=0;
int OldX,OldY,NewX,NewY;
float Elevation=0;
float Azimuth=0;
float DistX=0;
float DistY=0;
float DistZ=2;

// VISUALIZATION SETTINGS

float boundingBoxScale=1.0f;
float boundingBoxTranslateX = 0.0f;
float boundingBoxTranslateY = 0.0f;
float boundingBoxTranslateZ = 0.0f;

// GLOBAL CONTROL VARIABLES

int WindowW=1024, WindowH=768;
//int WindowW=800, WindowH=600;
//int WindowW=1280, WindowH=960;
int InteractionMode=0;
int AnimationOn=0;
int WorkingOn=0;
int PrintToFileOn=0;

// COLORS

bool switch_colors = true;

float colours_diffuse[10][4];
float colours_white[4];
float colours_light_blue[4];
float colours_red_white[4];
float colours_stripe_white[4];
float colours_stripe_black[4];
float colours_slow_change[4];

// DATA STORAGE FOR STREAM VISUALIZER OUTPUT

char* file_name = "models/dragon/dragon-spectral.smc";
bool isma = false;
bool ismb = false;
bool ismc = false;
bool ismc_old = false;
bool ismd = false;
bool isme = false;
SMreader* smreader = 0;
PositionQuantizer* pq = 0;

int nverts;
int nfaces;
int max_vertex_span;

int EXACTLY_N_STEPS = 100;
int EVERY_NTH_STEP = -1;
int TOTAL_STEPS;
int NEXT_STEP;

int DIRTY_MESH=1;
int DIRTY_VERTEX_SPAN = 1;
int DIRTY_LINES = 1;
int REPLAY_IT=0;
int REPLAY_COUNT=0;
int GRID_PRECISION=8;
int COMPUTE_VERTEX_SPAN=0;
int RED_SCALE = 2;
int STREAM_COLORING = 0;
int NUMBER_STRIPES = 20;
int STRIPE_WHITE=0;
int RENDER_MODE = 0;

int SHOW_VERTEX_SPAN=0;
int SHOW_LINES=0;

unsigned char* Framebuffer = 0;
char* PrintFileName = "frame";
int Time=0;

// efficient memory allocation

typedef struct GridVertex
{
  float v[3];
  int number;
  int index;
  unsigned short v_span;
} GridVertex;

typedef struct RenderVertex
{
  RenderVertex* buffer_next;
  float v[3];
} RenderVertex;

typedef hash_map<int, int> my_hash;
typedef hash_map<int, RenderVertex*> my_other_hash;

static my_hash* map_hash;
static int index_map_size = 0;
static int index_map_maxsize = 0;

static my_hash* grid_hash;
static GridVertex* grid_vertex_buffer = 0;
static int grid_vertex_buffer_size = 0;
static int grid_vertex_buffer_alloc = 0;

static void initGridVertexBuffer(int alloc)
{
  if (grid_vertex_buffer)
  {
    if (grid_vertex_buffer_alloc < alloc)
    {
      grid_vertex_buffer_alloc = alloc;
      free(grid_vertex_buffer);
      grid_vertex_buffer = (GridVertex*)malloc(sizeof(GridVertex)*grid_vertex_buffer_alloc);
    }
  }
  else
  {
    grid_vertex_buffer_alloc = alloc;
    grid_vertex_buffer = (GridVertex*)malloc(sizeof(GridVertex)*grid_vertex_buffer_alloc);
  }
  grid_vertex_buffer_size = 0;
}

static int allocGridVertex()
{
  if (grid_vertex_buffer_size == grid_vertex_buffer_alloc)
  {
    grid_vertex_buffer = (GridVertex*)realloc(grid_vertex_buffer,sizeof(GridVertex)*grid_vertex_buffer_alloc*2);
    if (!grid_vertex_buffer)
    {
      fprintf(stderr,"FATAL ERROR: realloc grid_vertex_buffer with %d failed.\n",grid_vertex_buffer_alloc*2);
      exit(0);
    }
    grid_vertex_buffer_alloc *= 2;
  }
  int index = grid_vertex_buffer_size;
  grid_vertex_buffer[index].v[0] = 0.0f;
  grid_vertex_buffer[index].v[1] = 0.0f;
  grid_vertex_buffer[index].v[2] = 0.0f;
  grid_vertex_buffer[index].index = index;
  grid_vertex_buffer[index].number = 0;
  grid_vertex_buffer[index].v_span = 0;
  grid_vertex_buffer_size++;
  return index;
}

static void destroyGridVertexBuffer()
{
  grid_vertex_buffer_size = 0;
  grid_vertex_buffer_alloc = 0;
  if (grid_vertex_buffer)
  {
    free(grid_vertex_buffer);
  }
  grid_vertex_buffer = 0;
}

static int* triangle_buffer = 0;
static int triangle_buffer_size = 0;
static int triangle_buffer_alloc = 0;

static void initTriangleBuffer(int alloc)
{
  if (triangle_buffer)
  {
    if (triangle_buffer_alloc < alloc)
    {
      triangle_buffer_alloc = alloc;
      free(triangle_buffer);
      triangle_buffer = (int*)malloc(sizeof(int)*triangle_buffer_alloc*3);
    }
  }
  else
  {
    triangle_buffer_alloc = alloc;
    triangle_buffer = (int*)malloc(sizeof(int)*triangle_buffer_alloc*3);
  }
  triangle_buffer_size = 0;
}

static int allocTriangle()
{
  if (triangle_buffer_size == triangle_buffer_alloc)
  {
    triangle_buffer = (int*)realloc(triangle_buffer,sizeof(int)*triangle_buffer_alloc*3*2);
    if (!triangle_buffer)
    {
      fprintf(stderr,"FATAL ERROR: realloc triangle_buffer with %d failed.\n",triangle_buffer_alloc*2);
      exit(0);
    }
    triangle_buffer_alloc *= 2;
  }
  int index = triangle_buffer_size;
  triangle_buffer_size++;
  return index;
}

static void destroyTriangleBuffer()
{
  triangle_buffer_size = 0;
  triangle_buffer_alloc = 0;
  if (triangle_buffer)
  {
    free(triangle_buffer);
  }
  triangle_buffer = 0;
}

static int render_vertex_buffer_alloc = 1024;
static RenderVertex* render_vertex_buffer_next = 0;

static RenderVertex* allocRenderVertex(float* v)
{
  if (render_vertex_buffer_next == 0)
  {
    render_vertex_buffer_next = (RenderVertex*)malloc(sizeof(RenderVertex)*render_vertex_buffer_alloc);
    if (render_vertex_buffer_next == 0)
    {
      fprintf(stderr,"malloc for render vertex buffer failed\n");
      return 0;
    }
    for (int i = 0; i < render_vertex_buffer_alloc; i++)
    {
      render_vertex_buffer_next[i].buffer_next = &(render_vertex_buffer_next[i+1]);
    }
    render_vertex_buffer_next[render_vertex_buffer_alloc-1].buffer_next = 0;
    render_vertex_buffer_alloc = 2*render_vertex_buffer_alloc;
  }
  // get pointer to next available vertex
  RenderVertex* vertex = render_vertex_buffer_next;
  render_vertex_buffer_next = vertex->buffer_next;

  VecCopy3fv(vertex->v, v);
  
  return vertex;
}

static void deallocRenderVertex(RenderVertex* vertex)
{
  vertex->buffer_next = render_vertex_buffer_next;
  render_vertex_buffer_next = vertex;
}

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
  
  colours_white[0] = 0.7f; colours_white[1] = 0.7f; colours_white[2] = 0.7f; colours_white[3] = 1.0f; // white
  colours_light_blue[0] = 0.2f; colours_light_blue[1] = 0.2f; colours_light_blue[2] = 0.6f; colours_light_blue[3] = 1.0f; // light blue
  colours_red_white[0] = 0.7f; colours_red_white[1] = 0.7f; colours_red_white[2] = 0.7f; colours_red_white[3] = 1.0f; // white or red

  colours_stripe_white[0] = 0.8f; colours_stripe_white[1] = 0.8f; colours_stripe_white[2] = 0.8f; colours_stripe_white[3] = 1.0f; // white
  colours_stripe_black[0] = 0.5f; colours_stripe_black[1] = 0.5f; colours_stripe_black[2] = 0.5f; colours_stripe_black[3] = 1.0f; // black

  colours_slow_change[0] = 0.5f; colours_slow_change[1] = 0.5f; colours_slow_change[2] = 0.5f; colours_stripe_black[3] = 1.0f; // black
} 

void InitLight()
{
  float intensity[] = {1,1,1,1};
  float position[] = {1,1,5,0}; // directional behind the viewer
  glLightfv(GL_LIGHT0,GL_DIFFUSE,intensity);
  glLightfv(GL_LIGHT0,GL_SPECULAR,intensity);
  glLightfv(GL_LIGHT0,GL_POSITION,position);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
}

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage()
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"sm_viewer mesh.sma\n");
  fprintf(stderr,"sm_viewer -ismc < mesh.smc\n");
  fprintf(stderr,"sm_viewer -win 640 480 \n");
  fprintf(stderr,"sm_viewer -win 1600 1200 mymesh.obj\n");
  fprintf(stderr,"sm_viewer -h\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"in addition to our formats SMA to SMD this tool\n");
  fprintf(stderr,"supports OBJ, SMF, PLY meshes (optionally gzipped).\n");
}

FILE* file = 0;

void vizBegin()
{
  REPLAY_IT = 0; // just making sure
  DIRTY_MESH = 1;
  DIRTY_LINES = 1;
  DIRTY_VERTEX_SPAN = 1;

  max_vertex_span = 0;

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

      if (smreader_sma->nfaces == -1 || smreader_sma->nverts == -1 || smreader_sma->bb_min_f == 0 || smreader_sma->bb_max_f == 0)
      {
        if (smreader_sma->bb_min_f && smreader_sma->bb_max_f)
        {
          fprintf(stderr,"need additional pass to count verts and faces\n");
          while (smreader_sma->read_element() > SM_EOF);
        }
        else
        {
          SMevent event;

          if (smreader_sma->nfaces == -1 || smreader_sma->nverts == -1)
          {
            fprintf(stderr,"need additional pass to count verts and faces and compute bounding box\n");
          }
          else
          {
            fprintf(stderr,"need additional pass to compute bounding box\n");
          }

          smreader_sma->bb_min_f = new float[3];
          smreader_sma->bb_max_f = new float[3];

          while ((event = smreader_sma->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecCopy3fv(smreader_sma->bb_min_f, smreader_sma->v_pos_f);
              VecCopy3fv(smreader_sma->bb_max_f, smreader_sma->v_pos_f);
              break;
            }
          }
          while ((event = smreader_sma->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecUpdateMinMax3fv(smreader_sma->bb_min_f, smreader_sma->bb_max_f, smreader_sma->v_pos_f);
            }
          }
          fprintf(stderr, "bb_min_f[0] = %ff; bb_min_f[1] = %ff; bb_min_f[2] = %ff;\n", smreader_sma->bb_min_f[0], smreader_sma->bb_min_f[1], smreader_sma->bb_min_f[2]);
          fprintf(stderr, "bb_max_f[0] = %ff; bb_max_f[1] = %ff; bb_max_f[2] = %ff;\n", smreader_sma->bb_max_f[0], smreader_sma->bb_max_f[1], smreader_sma->bb_max_f[2]);
        }
      
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

      if (smreader_smb->nfaces == -1 || smreader_smb->nverts == -1 || smreader_smb->bb_min_f == 0 || smreader_smb->bb_max_f == 0)
      {
        if (smreader_smb->bb_min_f && smreader_smb->bb_max_f)
        {
          fprintf(stderr,"need additional pass to count verts and faces\n");
          while (smreader_smb->read_element() > SM_EOF);
        }
        else
        {
          SMevent event;

          if (smreader_smb->nfaces == -1 || smreader_smb->nverts == -1)
          {
            fprintf(stderr,"need additional pass to count verts and faces and compute bounding box\n");
          }
          else
          {
            fprintf(stderr,"need additional pass to compute bounding box\n");
          }

          smreader_smb->bb_min_f = new float[3];
          smreader_smb->bb_max_f = new float[3];

          while ((event = smreader_smb->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecCopy3fv(smreader_smb->bb_min_f, smreader_smb->v_pos_f);
              VecCopy3fv(smreader_smb->bb_max_f, smreader_smb->v_pos_f);
              break;
            }
          }
          while ((event = smreader_smb->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecUpdateMinMax3fv(smreader_smb->bb_min_f, smreader_smb->bb_max_f, smreader_smb->v_pos_f);
            }
          }
          fprintf(stderr, "bb_min_f[0] = %ff; bb_min_f[1] = %ff; bb_min_f[2] = %ff;\n", smreader_smb->bb_min_f[0], smreader_smb->bb_min_f[1], smreader_smb->bb_min_f[2]);
          fprintf(stderr, "bb_max_f[0] = %ff; bb_max_f[1] = %ff; bb_max_f[2] = %ff;\n", smreader_smb->bb_max_f[0], smreader_smb->bb_max_f[1], smreader_smb->bb_max_f[2]);
        }

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

      if (smreader_smc_old->nfaces == -1 || smreader_smc_old->nverts == -1 || smreader_smc_old->bb_min_f == 0 || smreader_smc_old->bb_max_f == 0)
      {
        if (smreader_smc_old->bb_min_f && smreader_smc_old->bb_max_f)
        {
          fprintf(stderr,"need additional pass to count verts and faces\n");
          while (smreader_smc_old->read_element() > SM_EOF);
        }
        else
        {
          SMevent event;

          if (smreader_smc_old->nfaces == -1 || smreader_smc_old->nverts == -1)
          {
            fprintf(stderr,"need additional pass to count verts and faces and compute bounding box\n");
          }
          else
          {
            fprintf(stderr,"need additional pass to compute bounding box\n");
          }

          smreader_smc_old->bb_min_f = new float[3];
          smreader_smc_old->bb_max_f = new float[3];

          while ((event = smreader_smc_old->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecCopy3fv(smreader_smc_old->bb_min_f, smreader_smc_old->v_pos_f);
              VecCopy3fv(smreader_smc_old->bb_max_f, smreader_smc_old->v_pos_f);
              break;
            }
          }
          while ((event = smreader_smc_old->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecUpdateMinMax3fv(smreader_smc_old->bb_min_f, smreader_smc_old->bb_max_f, smreader_smc_old->v_pos_f);
            }
          }
          fprintf(stderr, "bb_min_f[0] = %ff; bb_min_f[1] = %ff; bb_min_f[2] = %ff;\n", smreader_smc_old->bb_min_f[0], smreader_smc_old->bb_min_f[1], smreader_smc_old->bb_min_f[2]);
          fprintf(stderr, "bb_max_f[0] = %ff; bb_max_f[1] = %ff; bb_max_f[2] = %ff;\n", smreader_smc_old->bb_max_f[0], smreader_smc_old->bb_max_f[1], smreader_smc_old->bb_max_f[2]);
        }
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

      if (smreader_smc->nfaces == -1 || smreader_smc->nverts == -1 || smreader_smc->bb_min_f == 0 || smreader_smc->bb_max_f == 0)
      {
        if (smreader_smc->bb_min_f && smreader_smc->bb_max_f)
        {
          fprintf(stderr,"need additional pass to count verts and faces\n");
          while (smreader_smc->read_element() > SM_EOF);
        }
        else
        {
          SMevent event;

          if (smreader_smc->nfaces == -1 || smreader_smc->nverts == -1)
          {
            fprintf(stderr,"need additional pass to count verts and faces and compute bounding box\n");
          }
          else
          {
            fprintf(stderr,"need additional pass to compute bounding box\n");
          }

          smreader_smc->bb_min_f = new float[3];
          smreader_smc->bb_max_f = new float[3];

          while ((event = smreader_smc->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecCopy3fv(smreader_smc->bb_min_f, smreader_smc->v_pos_f);
              VecCopy3fv(smreader_smc->bb_max_f, smreader_smc->v_pos_f);
              break;
            }
          }
          while ((event = smreader_smc->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecUpdateMinMax3fv(smreader_smc->bb_min_f, smreader_smc->bb_max_f, smreader_smc->v_pos_f);
            }
          }
          fprintf(stderr, "bb_min_f[0] = %ff; bb_min_f[1] = %ff; bb_min_f[2] = %ff;\n", smreader_smc->bb_min_f[0], smreader_smc->bb_min_f[1], smreader_smc->bb_min_f[2]);
          fprintf(stderr, "bb_max_f[0] = %ff; bb_max_f[1] = %ff; bb_max_f[2] = %ff;\n", smreader_smc->bb_max_f[0], smreader_smc->bb_max_f[1], smreader_smc->bb_max_f[2]);
        }
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

      if (smreader_smd->nfaces == -1 || smreader_smd->nverts == -1 || smreader_smd->bb_min_f == 0 || smreader_smd->bb_max_f == 0)
      {
        if (smreader_smd->bb_min_f && smreader_smd->bb_max_f)
        {
          fprintf(stderr,"need additional pass to count verts and faces\n");
          while (smreader_smd->read_element() > SM_EOF);
        }
        else
        {
          SMevent event;

          if (smreader_smd->nfaces == -1 || smreader_smd->nverts == -1)
          {
            fprintf(stderr,"need additional pass to count verts and faces and compute bounding box\n");
          }
          else
          {
            fprintf(stderr,"need additional pass to compute bounding box\n");
          }

          smreader_smd->bb_min_f = new float[3];
          smreader_smd->bb_max_f = new float[3];

          while ((event = smreader_smd->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecCopy3fv(smreader_smd->bb_min_f, smreader_smd->v_pos_f);
              VecCopy3fv(smreader_smd->bb_max_f, smreader_smd->v_pos_f);
              break;
            }
          }
          while ((event = smreader_smd->read_element()) > SM_EOF)
          {
            if (event == SM_VERTEX)
            {
              VecUpdateMinMax3fv(smreader_smd->bb_min_f, smreader_smd->bb_max_f, smreader_smd->v_pos_f);
            }
          }
          fprintf(stderr, "bb_min_f[0] = %ff; bb_min_f[1] = %ff; bb_min_f[2] = %ff;\n", smreader_smd->bb_min_f[0], smreader_smd->bb_min_f[1], smreader_smd->bb_min_f[2]);
          fprintf(stderr, "bb_max_f[0] = %ff; bb_max_f[1] = %ff; bb_max_f[2] = %ff;\n", smreader_smd->bb_max_f[0], smreader_smd->bb_max_f[1], smreader_smd->bb_max_f[2]);
        }
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
          // computing bounding box
          smreader_ply->open(file,true,true);
          smreader_ply->close();
          file = fopenGzipped(file_name, "rb");
          if (file == 0)
          {
            fprintf(stderr,"ERROR: cannot open %s\n",file_name);
            exit(1);
          }
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
        smreader_ply->open(file,true,false);
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
      ismc_old = false;
    }
    else if (ismd)
    {
      SMreader_smd* smreader_smd = new SMreader_smd();
      smreader_smd->open(file);
      smreader = smreader_smd;
      ismd = false;
    }
  }

  if (smreader->bb_min_f == 0 || smreader->bb_max_f == 0)
  {
    fprintf(stderr,"ERROR: no bounding box info ... exiting\n");
    exit(1);
  }

  if (smreader->post_order)
  {
    fprintf(stderr,"WARNING: input streaming mesh is post-order\n");
    fprintf(stderr,"         using 'readPostAsCompactPre' ...\n");
    SMreadPostAsCompactPre* smreadpostascompactpre = new SMreadPostAsCompactPre();
    smreadpostascompactpre->open(smreader);
    smreader = smreadpostascompactpre;
  }

  pq->SetPrecision(GRID_PRECISION);
  pq->SetMinMax(smreader->bb_min_f,smreader->bb_max_f);
  pq->SetupQuantizer();

  if ((smreader->bb_max_f[1]-smreader->bb_min_f[1]) > (smreader->bb_max_f[0]-smreader->bb_min_f[0]))
  {
    if ((smreader->bb_max_f[1]-smreader->bb_min_f[1]) > (smreader->bb_max_f[2]-smreader->bb_min_f[2]))
    {
      boundingBoxScale = 1.0f/(smreader->bb_max_f[1]-smreader->bb_min_f[1]);
    }
    else
    {
      boundingBoxScale = 1.0f/(smreader->bb_max_f[2]-smreader->bb_min_f[2]);
    }
  }
  else
  {
    if ((smreader->bb_max_f[0]-smreader->bb_min_f[0]) > (smreader->bb_max_f[2]-smreader->bb_min_f[2]))
    {
      boundingBoxScale = 1.0f/(smreader->bb_max_f[0]-smreader->bb_min_f[0]);
    }
    else
    {
      boundingBoxScale = 1.0f/(smreader->bb_max_f[2]-smreader->bb_min_f[2]);
    }
  }
  boundingBoxTranslateX = - boundingBoxScale * (smreader->bb_min_f[0] + 0.5f * (smreader->bb_max_f[0]-smreader->bb_min_f[0]));
  boundingBoxTranslateY = - boundingBoxScale * (smreader->bb_min_f[1] + 0.5f * (smreader->bb_max_f[1]-smreader->bb_min_f[1]));
  boundingBoxTranslateZ = - boundingBoxScale * (smreader->bb_min_f[2] + 0.5f * (smreader->bb_max_f[2]-smreader->bb_min_f[2]));

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

  initGridVertexBuffer(65536);
  initTriangleBuffer(65536*2);

  grid_hash->clear();
  map_hash->clear();
  index_map_maxsize = 0;
}

void vizEnd()
{
  REPLAY_IT = 0; // just making sure
  REPLAY_COUNT = triangle_buffer_size;
  DIRTY_MESH = 0;
  DIRTY_LINES = 1;
  if (COMPUTE_VERTEX_SPAN)
  {
    DIRTY_VERTEX_SPAN = 0;
    COMPUTE_VERTEX_SPAN = 0;
    fprintf(stderr,"... vertex_span is %2.2f %%\n", 100.0f*max_vertex_span/65536);
  }
  fprintf(stderr,"grid: %dx%dx%d\n", GRID_PRECISION, GRID_PRECISION, GRID_PRECISION);
  fprintf(stderr,"  simplified vertices: %d\n", grid_vertex_buffer_size);
  fprintf(stderr,"  simplified triangles: %d\n", triangle_buffer_size);
  fprintf(stderr,"maxsize of index hash_map: %d (i.e. front width %2.2f %%)\n", index_map_maxsize, ((float)index_map_maxsize)*100/smreader->v_count);
  index_map_maxsize = 0;

  smreader->close();
  fclose(file);
  delete smreader;

  smreader = 0;
}

int vizContinue()
{
  int i;
  int t_idx[3];
  SMevent element = SM_ERROR;
  my_hash::iterator hash_element;

  REPLAY_IT = 0; // just making sure

  while (element)
  {
    element = smreader->read_element();

    if (element == SM_TRIANGLE)
    {
      for (i = 0; i < 3; i++)
      {
        hash_element = map_hash->find(smreader->t_idx[i]);
        if (hash_element == map_hash->end())
        {
          fprintf(stderr,"FATAL ERROR: index not in index map hash.\n");
          fprintf(stderr,"             either the streaming mesh is not in\n");
          fprintf(stderr,"             pre-order or it is corrupt.\n");
          exit(0);
        }
        else
        {
          t_idx[i] = (*hash_element).second;
        }
        if (smreader->t_final[i])
        {
          map_hash->erase(hash_element);
          index_map_size--;
        }
      }
      if (COMPUTE_VERTEX_SPAN)
      {
        unsigned short v_span = (unsigned short)((((float)(VecMax3iv(smreader->t_idx) - VecMin3iv(smreader->t_idx)))/((float)nverts))*65535);
        if (v_span > max_vertex_span) max_vertex_span = v_span;

        for (i = 0; i < 3; i++)
        {
          if (v_span > (grid_vertex_buffer[t_idx[i]].v_span))
          {
            grid_vertex_buffer[t_idx[i]].v_span = v_span;
          }
        }
      }
      if (t_idx[0] != t_idx[1] && t_idx[0] != t_idx[2] && t_idx[1] != t_idx[2])
      {
        int triangle_idx = allocTriangle();
        VecCopy3iv(&(triangle_buffer[triangle_idx*3]),t_idx);
      }
    }
    else if (element == SM_VERTEX)
    {
      int grid_pos[3];
      int grid_idx, vertex_idx;

      pq->EnQuantize(smreader->v_pos_f, grid_pos);
      grid_idx = (grid_pos[0] << (GRID_PRECISION+GRID_PRECISION)) + (grid_pos[1] << GRID_PRECISION) + (grid_pos[2]);

      // check if grid vertex for the grid cell of this vertex already exists
      hash_element = grid_hash->find(grid_idx);
      if (hash_element == grid_hash->end())
      {
        vertex_idx = allocGridVertex();
      // all following vertices falling into this grid cell find their grid vertex in the grid_hash
        grid_hash->insert(my_hash::value_type(grid_idx, vertex_idx));
      }
      else
      {
        vertex_idx = (*hash_element).second;
      }

      // all following triangles can find this vertex in the map_hash
      map_hash->insert(my_hash::value_type(smreader->v_idx, vertex_idx));
      index_map_size++;
      if (index_map_size > index_map_maxsize) index_map_maxsize = index_map_size;

      VecSelfAdd3fv(grid_vertex_buffer[vertex_idx].v, smreader->v_pos_f);
      grid_vertex_buffer[vertex_idx].number++;
    }
    else if (element == SM_FINALIZED)
    {
      hash_element = map_hash->find(smreader->final_idx);
      if (hash_element == map_hash->end())
      {
        fprintf(stderr,"FATAL ERROR: non-existing vertex was explicitely finalized.\n");
        fprintf(stderr,"             the streaming mesh is corrupt.\n");
        exit(0);
      }
      else
      {
        map_hash->erase(hash_element);
        index_map_size--;
      }
    }
    else if (element == SM_EOF)
    {
      index_map_size = 0;

      grid_hash->clear();
      map_hash->clear();
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
  else if (REPLAY_IT)
  {
    REPLAY_COUNT += NEXT_STEP;
    glutPostRedisplay();
  }
}

void compute_lines()
{
  int i;

  if (triangle_buffer_size)
  {
    for (i = 0; i < grid_vertex_buffer_size; i++)
    {
      grid_vertex_buffer[i].v_span = 0;
    }
    for (i = 0; i < triangle_buffer_size; i++)
    {
      if ((i % (triangle_buffer_size/NUMBER_STRIPES)) == 0)
      {
        STRIPE_WHITE = !STRIPE_WHITE;
      }
      if (STRIPE_WHITE)
      {
        for (int j = 0; j < 3; j++) grid_vertex_buffer[triangle_buffer[3*i+j]].v_span |= 1;
      }
      else
      {
        for (int j = 0; j < 3; j++) grid_vertex_buffer[triangle_buffer[3*i+j]].v_span |= 2;
      }
    }
  }
}

void full_resolution_rendering()
{
  if (file_name == 0)
  {
    fprintf(stderr,"ERROR: no input file\n");
  }

  int f_count;

  if (smreader)
  {
    f_count = smreader->f_count;
    fprintf(stderr,"out-of-core rendering of %d mesh faces ... \n",f_count);
    smreader->close();
    fclose(file);
    delete smreader;
    smreader = 0;
  }
  else
  {
    f_count = 2000000000;
    fprintf(stderr,"out-of-core rendering of mesh ... \n");
  }


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
      smreader_ply->open(file);
      smreader = smreader_ply;
    }
  }
  else
  {
    fprintf(stderr,"ERROR: cannot guess input type from file name %s \n", file_name);
    exit(1);
  }

  my_other_hash* render_hash = new my_other_hash;

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glViewport(0,0,WindowW,WindowH);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0f,(float)WindowW/WindowH,0.0625f,5.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(DistX,DistY,DistZ, DistX,DistY,0, 0,1,0);

  glRotatef(Elevation,1,0,0);
  glRotatef(Azimuth,0,1,0);

  glTranslatef(boundingBoxTranslateX,boundingBoxTranslateY,boundingBoxTranslateZ);
  glScalef(boundingBoxScale,boundingBoxScale,boundingBoxScale);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_NORMALIZE);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  if (switch_colors)
  {
    glMaterialfv(GL_FRONT, GL_DIFFUSE, colours_white);
    glMaterialfv(GL_BACK, GL_DIFFUSE, colours_light_blue);
  }
  else
  {
    glMaterialfv(GL_FRONT, GL_DIFFUSE, colours_light_blue);
    glMaterialfv(GL_BACK, GL_DIFFUSE, colours_white);
  }

  if (f_count == 0)
  {
    f_count = smreader->nfaces;
  }

  int i;
  float n[3];
  RenderVertex* v[3];
  my_other_hash::iterator hash_element;
  glBegin(GL_TRIANGLES);
  while (smreader->f_count < f_count)
  {
    switch (smreader->read_element())
    {
    case SM_TRIANGLE:
      for (i = 0; i < 3; i++)
      {
        hash_element = render_hash->find(smreader->t_idx[i]);
        if (hash_element == render_hash->end())
        {
          fprintf(stderr,"FATAL ERROR: index not in render map hash.\n");
          fprintf(stderr,"             either the streaming mesh is not in\n");
          fprintf(stderr,"             pre-order or it is corrupt.\n");
          exit(0);
        }
        else
        {
          v[i] = (*hash_element).second;
        }
        if (smreader->t_final[i])
        {
          deallocRenderVertex(v[i]);
          render_hash->erase(hash_element);
        }
      }
      VecCcwNormal3fv(n, v[0]->v, v[1]->v, v[2]->v);
      glNormal3fv(n);
      glVertex3fv(v[0]->v);
      glVertex3fv(v[1]->v);
      glVertex3fv(v[2]->v);
      break;
    case SM_VERTEX:
      v[0] = allocRenderVertex(smreader->v_pos_f);
      render_hash->insert(my_other_hash::value_type(smreader->v_idx, v[0]));
      break;
    case SM_FINALIZED:
      hash_element = render_hash->find(smreader->final_idx);
      if (hash_element == render_hash->end())
      {
        fprintf(stderr,"FATAL ERROR: index not in render map hash.\n");
        fprintf(stderr,"             either the streaming mesh is not in\n");
        fprintf(stderr,"             pre-order or it is corrupt.\n");
        exit(0);
      }
      deallocRenderVertex((*hash_element).second);
      render_hash->erase(hash_element);
      break;
    case SM_EOF:
      glEnd();
      smreader->close();
      fclose(file);
      delete smreader;
      smreader = 0;
      glutSwapBuffers();
      return;
    }
  }
  glEnd();
  glutSwapBuffers();
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
    if (InteractionMode == 0)
    {
      Azimuth += (RelX*180);
      Elevation += (RelY*180);
    }
    else if (InteractionMode == 1)
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
  case ' ': // rotate, translate, or zoom
    if (InteractionMode == 2)
    {
      InteractionMode = 0;
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
    RED_SCALE /= 2;
    if (RED_SCALE < 1) RED_SCALE = 1;
    fprintf(stderr,"RED_SCALE %d\n",RED_SCALE);
    glutPostRedisplay();
    break;
  case '=':
    RED_SCALE *= 2;
    fprintf(stderr,"RED_SCALE %d\n",RED_SCALE);
    glutPostRedisplay();
    break;
  case 'C':
  case 'c':
    STREAM_COLORING = (STREAM_COLORING+1)%4;
    fprintf(stderr,"STREAM_COLORING %d\n",STREAM_COLORING);
    glutPostRedisplay();
    break;
  case 'M':
  case 'm':
    RENDER_MODE = (RENDER_MODE+1)%3;
    fprintf(stderr,"RENDER_MODE %d\n",RENDER_MODE);
    glutPostRedisplay();
    break;
  case 'O':
  case 'o':
    AnimationOn = 0;
    REPLAY_IT = 0;
    break;
  case 'V':
  case 'v':
    fprintf(stderr,"SHOW_VERTEX_SPAN %d\n",!SHOW_VERTEX_SPAN);
    if (SHOW_VERTEX_SPAN)
    {
      SHOW_VERTEX_SPAN = 0;
      glutPostRedisplay();
    }
    else
    {
      SHOW_VERTEX_SPAN = 1;
      if (DIRTY_VERTEX_SPAN)
      {
        COMPUTE_VERTEX_SPAN = 1;
        DIRTY_MESH = 1;
        fprintf(stderr,"computing vertex_span ...\n");
        myKeyboard('p', 0, 0);
      }
      else
      {
        glutPostRedisplay();
      }
    }
    break;
  case 'R':
  case 'r':
    full_resolution_rendering();
    break;
  case 'W':
    break;
  case 'w':
    switch_colors = !switch_colors;
    glutPostRedisplay();
    break;
  case 'D':
  case 'd':
    PrintToFileOn=1;
    if (Framebuffer) delete [] Framebuffer;
    Framebuffer = new unsigned char[WindowW*WindowH*3];
    fprintf(stderr,"print_to_file %d\n",PrintToFileOn);
    break;
  case 'T':
    if (DIRTY_MESH)
    {
      // works only in replay mode
      fprintf(stderr,"tiny steps only work during second play (replay)\n");
    }
    else
    {
      REPLAY_COUNT -= 1;
      if (REPLAY_COUNT < 0)
      {
        REPLAY_COUNT = 0;
      }
    }
    glutPostRedisplay();
    break;
  case 't':
    if (DIRTY_MESH)
    {
      // works only in replay mode
      fprintf(stderr,"tiny steps only work during second play (replay)\n");
    }
    else
    {
      if (REPLAY_COUNT >= triangle_buffer_size)
      {
        REPLAY_COUNT = 0;
      }
      REPLAY_COUNT += 1;
    }
    glutPostRedisplay();
    break;
  case 'S':
    if (DIRTY_MESH)
    {
      // works only in replay mode
      fprintf(stderr,"back stepping only work during second play (replay)\n");
    }
    else
    {
      NEXT_STEP = triangle_buffer_size / EXACTLY_N_STEPS;
      if (NEXT_STEP == 0) NEXT_STEP = 1;
      REPLAY_COUNT -= NEXT_STEP;
      if (REPLAY_COUNT < 0)
      {
        REPLAY_COUNT = 0;
      }
    }
    glutPostRedisplay();
    break;
  case 'P':
      DIRTY_MESH = 1;
  case 'p':
    if (DIRTY_MESH)
    {
      AnimationOn = !AnimationOn;
    }
    else
    {
      if (REPLAY_IT == 0)
      {
        if (REPLAY_COUNT >= triangle_buffer_size)
        {
          REPLAY_COUNT = 0;
        }
        NEXT_STEP = triangle_buffer_size / EXACTLY_N_STEPS;
        if (NEXT_STEP == 0) NEXT_STEP = 1;
        REPLAY_IT = 1;
      }
      else
      {
        REPLAY_IT = 0;
      }
    }
  case 's':
    if (DIRTY_MESH)
    {
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
    }
    else
    {
      if (REPLAY_COUNT >= triangle_buffer_size)
      {
        REPLAY_COUNT = 0;
      }
      NEXT_STEP = triangle_buffer_size / EXACTLY_N_STEPS;
      if (NEXT_STEP == 0) NEXT_STEP = 1;
      REPLAY_COUNT += NEXT_STEP;
    }
    glutPostRedisplay();
    break;
  case 'K':
  case 'k':
    printf("Azimuth = %ff;\n",Azimuth);
    printf("Elevation = %ff;\n",Elevation);
    printf("DistX = %ff; DistY = %ff; DistZ = %ff;\n",DistX,DistY,DistZ);
    break;
  case 'L':
  case 'l':
    fprintf(stderr,"SHOW_LINES %d\n",!SHOW_LINES);
    if (SHOW_LINES)
    {
      SHOW_LINES = 0;
    }
    else
    {
      SHOW_LINES = 1;
      if (DIRTY_LINES)
      {
        fprintf(stderr,"computing lines ...\n");
        compute_lines();
        DIRTY_LINES = 0;
        DIRTY_VERTEX_SPAN = 1;
        SHOW_VERTEX_SPAN = 0;
      }
    }
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
    else if (value == 150)
    {
      myKeyboard('v',0,0);
    }
    else if (value == 151)
    {
      myKeyboard('l',0,0);
    }
    else if (value == 152)
    {
      myKeyboard('c',0,0);
    }
    else if (value == 153)
    {
      myKeyboard('m',0,0);
    }
  }
  else if (value == 1)
  {
    file_name = "models/dragon/dragon-depthfirst.oocc";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/dragon/dragon-breadthfirst.oocc";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/dragon/dragon-zorder.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/dragon/dragon-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/dragon/dragon-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/bunny/bunny-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/armadillo/armadillo-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/dragon/dragon-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 9)
  {
    file_name = "models/buddha/buddha-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 10)
  {
    file_name = "models/asian-dragon/asian-dragon-vcompact.smc";
    DIRTY_MESH = 1;
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
  else if (value == 71)
  {
    if (GRID_PRECISION != 3) DIRTY_MESH = 1;
    GRID_PRECISION = 3;
  }
  else if (value == 72)
  {
    if (GRID_PRECISION != 4) DIRTY_MESH = 1;
    GRID_PRECISION = 4;
  }
  else if (value == 73)
  {
    if (GRID_PRECISION != 5) DIRTY_MESH = 1;
    GRID_PRECISION = 5;
  }
  else if (value == 74)
  {
    if (GRID_PRECISION != 6) DIRTY_MESH = 1;
    GRID_PRECISION = 6;
  }
  else if (value == 75)
  {
    if (GRID_PRECISION != 7) DIRTY_MESH = 1;
    GRID_PRECISION = 7;
  }
  else if (value == 76)
  {
    if (GRID_PRECISION != 8) DIRTY_MESH = 1;
    GRID_PRECISION = 8;
  }
  else if (value == 77)
  {
    if (GRID_PRECISION != 9) DIRTY_MESH = 1;
    GRID_PRECISION = 9;
  }
  else if (value == 78)
  {
    if (GRID_PRECISION != 10) DIRTY_MESH = 1;
    GRID_PRECISION = 10;
  }
  else if (value == 81)
  {
    if (NUMBER_STRIPES != 10) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 10;
  }
  else if (value == 82)
  {
    if (NUMBER_STRIPES != 15) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 15;
  }
  else if (value == 83)
  {
    if (NUMBER_STRIPES != 20) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 20;
  }
  else if (value == 84)
  {
    if (NUMBER_STRIPES != 25) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 25;
  }
  else if (value == 85)
  {
    if (NUMBER_STRIPES != 40) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 40;
  }
  else if (value == 86)
  {
    if (NUMBER_STRIPES != 50) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 50;
  }
  else if (value == 87)
  {
    if (NUMBER_STRIPES != 60) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 60;
  }
  else if (value == 88)
  {
    if (NUMBER_STRIPES != 75) {DIRTY_LINES = 1; SHOW_LINES = 0;}
    NUMBER_STRIPES = 75;
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
  
  if( InteractionMode == 0 )
  {
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'r');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'o');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'a');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'e');
  }
  else if( InteractionMode == 1 )
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

void render_triangle(GridVertex* vertex0, GridVertex* vertex1, GridVertex* vertex2)
{
  float n[3];
  float v0[3];
  float v1[3];
  float v2[3];

  VecScalarDiv3fv(v0, vertex0->v, (float)vertex0->number);
  VecScalarDiv3fv(v1, vertex1->v, (float)vertex1->number);
  VecScalarDiv3fv(v2, vertex2->v, (float)vertex2->number);
  VecCcwNormal3fv(n, v0, v1, v2);

  glNormal3fv(n);
  glVertex3fv(v0);
  glVertex3fv(v1);
  glVertex3fv(v2);
}

void render_line(GridVertex* vertex0, GridVertex* vertex1)
{
  float v0[3];
  float v1[3];

  VecScalarDiv3fv(v0, vertex0->v, (float)vertex0->number);
  VecScalarDiv3fv(v1, vertex1->v, (float)vertex1->number);

  glVertex3fv(v0);
  glVertex3fv(v1);
}

void myDisplay()
{
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glViewport(0,0,WindowW,WindowH);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0f,(float)WindowW/WindowH,0.0625f,5.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(DistX,DistY,DistZ, DistX,DistY,0, 0,1,0);

  glRotatef(Elevation,1,0,0);
  glRotatef(Azimuth,0,1,0);

  glTranslatef(boundingBoxTranslateX,boundingBoxTranslateY,boundingBoxTranslateZ);
  glScalef(boundingBoxScale,boundingBoxScale,boundingBoxScale);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_NORMALIZE);

  if (RENDER_MODE == 0)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  else if (RENDER_MODE == 1)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  else if (RENDER_MODE == 2)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
  }

  if (switch_colors)
  {
    glMaterialfv(GL_FRONT, GL_DIFFUSE, colours_white);
    glMaterialfv(GL_BACK, GL_DIFFUSE, colours_light_blue);
  }
  else
  {
    glMaterialfv(GL_FRONT, GL_DIFFUSE, colours_light_blue);
    glMaterialfv(GL_BACK, GL_DIFFUSE, colours_white);
  }
  
  if (SHOW_LINES)
  {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glLineWidth(2.0f);
  }

  STRIPE_WHITE = 0;

  int rendered_triangles;

  if (DIRTY_MESH)
  {
    rendered_triangles = triangle_buffer_size;
  }
  else
  {
    if (REPLAY_COUNT > triangle_buffer_size)
    {
      rendered_triangles = triangle_buffer_size;
      REPLAY_IT = 0;
    }
    else
    {
      rendered_triangles = REPLAY_COUNT;
    }
  }

  // draw triangles

  if (triangle_buffer_size)
  {
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < rendered_triangles; i++)
    {
      if (SHOW_VERTEX_SPAN)
      {
        int v_span_max = grid_vertex_buffer[triangle_buffer[3*i+0]].v_span;
        if (v_span_max < grid_vertex_buffer[triangle_buffer[3*i+1]].v_span) v_span_max = grid_vertex_buffer[triangle_buffer[3*i+1]].v_span;
        if (v_span_max < grid_vertex_buffer[triangle_buffer[3*i+2]].v_span) v_span_max = grid_vertex_buffer[triangle_buffer[3*i+2]].v_span;
        float red = 0.7f-(0.7f/65535*RED_SCALE*v_span_max);
        colours_red_white[1] = red;
        colours_red_white[2] = red;

        if (switch_colors)
        {
          glMaterialfv(GL_FRONT, GL_DIFFUSE,colours_red_white);
        }
        else
        {
          glMaterialfv(GL_BACK, GL_DIFFUSE, colours_red_white);
        }
      }
      else if (STREAM_COLORING)
      {
        if (STREAM_COLORING == 1)
        {
          if ((i % (triangle_buffer_size/NUMBER_STRIPES)) == 0)
          {
            if (STRIPE_WHITE)
            {
              glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,colours_stripe_white);
            }
            else
            {
              glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_stripe_black);
            }
            STRIPE_WHITE = !STRIPE_WHITE;
          }
        }
        else if (STREAM_COLORING == 2)
        {
          colours_slow_change[0] = 0.1f+0.8f*i/triangle_buffer_size;
          colours_slow_change[1] = colours_slow_change[0];
          colours_slow_change[2] = colours_slow_change[0];
          glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_slow_change);
        }
        else if (STREAM_COLORING == 3)
        {
          if (i < triangle_buffer_size/3)
          {
            colours_slow_change[0] = 0.1f+0.8f*i/(triangle_buffer_size/3);
            colours_slow_change[1] = 0.1f;
            colours_slow_change[2] = 0.1f;
          }
          else if (i < 2*(triangle_buffer_size/3))
          {
            colours_slow_change[0] = 0.9f;
            colours_slow_change[1] = 0.1f+0.8f*(i-(triangle_buffer_size/3))/(triangle_buffer_size/3);
            colours_slow_change[2] = 0.1f;
          }
          else
          {
            colours_slow_change[0] = 0.9f;
            colours_slow_change[1] = 0.9f;
            colours_slow_change[2] = 0.1f+0.8f*(i-2*(triangle_buffer_size/3))/(triangle_buffer_size/3);
          }
          glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_slow_change);
        }
      }
      render_triangle(&(grid_vertex_buffer[triangle_buffer[3*i+0]]), &(grid_vertex_buffer[triangle_buffer[3*i+1]]), &(grid_vertex_buffer[triangle_buffer[3*i+2]]));
    }
    glEnd();
  }

  glDisable(GL_LIGHTING);
  glDisable(GL_LIGHT0);
  glDisable(GL_NORMALIZE);

  if (SHOW_LINES)
  {
    glColor3fv(colours_diffuse[0]);
    glBegin(GL_LINES);
    for (int i = 0; i < triangle_buffer_size; i++)
    {
      for (int j = 0; j < 3; j++)
      {
        if ((grid_vertex_buffer[triangle_buffer[3*i+j]].v_span == 3) && (grid_vertex_buffer[triangle_buffer[3*i+((j+1)%3)]].v_span == 3))
        {
          render_line(&(grid_vertex_buffer[triangle_buffer[3*i+j]]), &(grid_vertex_buffer[triangle_buffer[3*i+((j+1)%3)]]));
        }
      }
    }
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  glDisable(GL_DEPTH_TEST);
  
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

void MyBunnyFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/bunny/bun_zipper.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/bunny/bunny-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/bunny/bunny-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/bunny/bunny-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/bunny/bunny-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/bunny/bunny-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/bunny/bunny-depth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/bunny/bunny-rendering.smc";
    DIRTY_MESH = 1;
  }
}

void MyHorseFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/horse/horse.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/horse/horse-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/horse/horse-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/horse/horse-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/horse/horse-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/horse/horse-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/horse/horse-depth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/horse/horse-rendering.smc";
    DIRTY_MESH = 1;
  }
}

void MyDinosaurFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/dinosaur/dinosaur.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/dinosaur/dinosaur-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/dinosaur/dinosaur-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/dinosaur/dinosaur-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/dinosaur/dinosaur-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/dinosaur/dinosaur-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/dinosaur/dinosaur-depth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/dinosaur/dinosaur-rendering.smc";
    DIRTY_MESH = 1;
  }
}

void MyArmadilloFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/armadillo/armadillo.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/armadillo/armadillo-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/armadillo/armadillo-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/armadillo/armadillo-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/armadillo/armadillo-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/armadillo/armadillo-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/armadillo/armadillo-depth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/armadillo/armadillo-rendering.smc";
    DIRTY_MESH = 1;
  }
}

void MyDragonFunc(int value)
{
  if (value == 1)
  {
    DIRTY_MESH = 1;
    file_name = "models/dragon/dragon_vrip.ply.gz";
  }
  else if (value == 2)
  {
    file_name = "models/dragon/dragon-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/dragon/dragon-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/dragon/dragon-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/dragon/dragon-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/dragon/dragon-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/dragon/dragon-depth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/dragon/dragon-rendering.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 9)
  {
    file_name = "models/dragon/dragon-depthfirst.oocc";
    DIRTY_MESH = 1;
  }
  else if (value == 10)
  {
    file_name = "models/dragon/dragon-breadthfirst.oocc";
    DIRTY_MESH = 1;
  }
  else if (value == 11)
  {
    file_name = "models/dragon/dragon-zorder.smc";
    DIRTY_MESH = 1;
  }
}

void MyBuddhaFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/buddha/happy_vrip.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/buddha/buddha-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/buddha/buddha-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/buddha/buddha-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/buddha/buddha-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/buddha/buddha-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/buddha/buddha-depth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 8)
  {
    file_name = "models/buddha/buddha-rendering.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 9)
  {
    file_name = "models/buddha/buddha-breadthfirst.oocc";
    DIRTY_MESH = 1;
  }
  else if (value == 10)
  {
    file_name = "models/buddha/buddha-depthfirst.oocc";
    DIRTY_MESH = 1;
  }
}

void MyAsianDragonFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/asian-dragon/asian-dragon.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/asian-dragon/asian-dragon-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/asian-dragon/asian-dragon-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/asian-dragon/asian-dragon-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/asian-dragon/asian-dragon-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/asian-dragon/asian-dragon-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/asian-dragon/asian-dragon-depth.smc";
    DIRTY_MESH = 1;
  }
}

void MyThaiStatueFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/thai-statue/thai-statue.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/thai-statue/thai-statue-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/thai-statue/thai-statue-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/thai-statue/thai-statue-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/thai-statue/thai-statue-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/thai-statue/thai-statue-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/thai-statue/thai-statue-depth.smc";
    DIRTY_MESH = 1;
  }
}

void MyLucyFunc(int value)
{
  if (value == 1)
  {
    file_name = "models/lucy/lucy.ply.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 2)
  {
    file_name = "models/lucy/lucy-interleaved.smb.gz";
    DIRTY_MESH = 1;
  }
  else if (value == 3)
  {
    file_name = "models/lucy/lucy-vcompact.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 4)
  {
    file_name = "models/lucy/lucy-spectral.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 5)
  {
    file_name = "models/lucy/lucy-geometric.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 6)
  {
    file_name = "models/lucy/lucy-breadth.smc";
    DIRTY_MESH = 1;
  }
  else if (value == 7)
  {
    file_name = "models/lucy/lucy-depth.smc";
    DIRTY_MESH = 1;
  }
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

  pq = new PositionQuantizer();
  grid_hash = new my_hash;
  map_hash = new my_hash;

  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(WindowW,WindowH);
  glutInitWindowPosition(180,100);
  glutCreateWindow("Streaming Mesh Viewer");
  
  glShadeModel(GL_FLAT);
  
  InitColors();
  InitLight();
  
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

  // points sub menu
  int menuLines = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("10 lines", 81);
  glutAddMenuEntry("15 lines", 82);
  glutAddMenuEntry("20 lines", 83);
  glutAddMenuEntry("25 lines", 84);
  glutAddMenuEntry("40 lines", 85);
  glutAddMenuEntry("50 lines", 86);
  glutAddMenuEntry("60 lines", 87);
  glutAddMenuEntry("75 lines", 88);

  // points sub menu
  int menuGrid = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("3x3x3 grid", 71);
  glutAddMenuEntry("4x4x4 grid", 72);
  glutAddMenuEntry("5x5x5 grid", 73);
  glutAddMenuEntry("6x6x6 grid", 74);
  glutAddMenuEntry("7x7x7 grid", 75);
  glutAddMenuEntry("8x8x8 grid", 76);
  glutAddMenuEntry("9x9x9 grid", 77);
  glutAddMenuEntry("10x10x10 grid", 78);

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
  glutAddMenuEntry("original PLY (slow!)", 1);
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
  glutAddMenuEntry("original PLY (slow!)", 1);
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
  glutAddMenuEntry("original PLY (slow!!!!!)", 1);
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
  glutAddMenuEntry("dragon (spectral) <5>", 5);
  glutAddMenuEntry("", 0);
  glutAddSubMenu("bunny ... <6>", menuBunny);
  glutAddSubMenu("horse ...", menuHorse);
  glutAddSubMenu("dinosaur ...", menuDinosaur);
  glutAddSubMenu("armadillo ... <7>", menuArmadillo);
  glutAddSubMenu("dragon ... <8>", menuDragon);
  glutAddSubMenu("buddha ... <9>", menuBuddha);
  glutAddSubMenu("asian-dragon ... ", menuAsianDragon);
  glutAddSubMenu("thai-statue ...", menuThaiStatue);
  glutAddSubMenu("lucy... <0>", menuLucy);

  // main menu
  glutCreateMenu(MyMenuFunc);
  glutAddSubMenu("models ...", menuModels);
  glutAddSubMenu("grid ...", menuGrid);
  glutAddMenuEntry("", 0);
  glutAddSubMenu("steps ...", menuSteps);
  glutAddSubMenu("lines ...", menuLines);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("rotate <SPACE>", 100);
  glutAddMenuEntry("translate <SPACE>", 101);
  glutAddMenuEntry("zoom <SPACE>", 102);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<s>tep", 103);
  glutAddMenuEntry("<p>lay", 104);
  glutAddMenuEntry("st<o>p", 105);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<v>ertex span (+/-)", 150);
  glutAddMenuEntry("stream-<l>ines", 151);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("stream <c>oloring", 152);
  glutAddMenuEntry("render <m>ode", 153);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<Q>UIT", 109);
  glutAttachMenu(GLUT_RIGHT_BUTTON);

  glutMainLoop();

  return 0;
}
