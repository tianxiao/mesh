/*
===============================================================================

  FILE:  PSconverter.cpp
  
  CONTENTS:
  
    see corresponding header file
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    see corresponding header file
  
===============================================================================
*/
#include "psconverter.h"

#define PS_RING_START 32 // not exposed by API right now
#define PS_RING_END   64 // not exposed by API right now

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec3fv.h"
#include "vec3iv.h"

#include <hash_map>

#define PRINT_CONTROL_OUTPUT
#undef PRINT_CONTROL_OUTPUT

#define USE_VDATA 1
#define USE_EDATA 1

typedef struct PSconnectivityVertex
{
  float v[3];
  int index;
  int list_size;
  int list_alloc;
  int* list;
} PSconnectivityVertex;

typedef struct PSoutputVertex
{
  float v[3];
  int index;
  int original;
  int vflag;
  int use_count;
  const void* user_data;
  int non_manifold;
} PSoutputVertex;

typedef std::hash_map<int, int> my_pscv_hash;

static my_pscv_hash* pscv_hash;

// output lists

static int outlist0;
static int outlist1;
static int outlist2;
static int outlist3;

static int outlist0_available;
static int outlist1_available;
static int outlist2_available;
static int outlist3_available;

static int output_triangles_available;

static int output_triangles_buffer_high;
static int output_triangles_buffer_low;

static int pscv_completed;

// some defines

#define PS_MANIFOLD_EDGE 0
#define PS_NOT_INITIALIZED -1
#define PS_BORDER_EDGE -2
#define PS_NON_MANIFOLD_EDGE -3
#define PS_NOT_ORIENTED_EDGE -4
#define PS_OUTPUT_BOUNDARY_EDGE -5

// statistics

#ifdef PRINT_CONTROL_OUTPUT

static int output_type0;
static int output_type1;
static int output_type2;
static int output_type3;

static int pscv_buffer_size;
static int psov_buffer_size;
static int triangle_buffer_size;

static int pscv_buffer_maxsize;
static int psov_buffer_maxsize;
static int triangle_buffer_maxsize;

static int number_border_edges;
static int number_manifold_edges;
static int number_non_manifold_edges;
static int number_not_oriented_edges;

#endif

// efficient memory allocation

static int triangle_buffer_alloc;
static int triangle_buffer_next;
static int* twinorigin;
static int* twininv;
static char* complete;
static int* original;
static int* next_in_outlist;
static int* prev_in_outlist;

static const void** buffer_edata;

int PSconverter::init_triangle_buffer(int size)
{
  twinorigin = (int*)malloc(sizeof(int)*size*3);
  if (twinorigin == 0)
  {
    fprintf(stderr,"ERROR: malloc for twinorigin failed\n");
    return 0;
  }
  twininv = (int*)malloc(sizeof(int)*size*3);
  if (twininv == 0)
  {
    fprintf(stderr,"ERROR: malloc for twininv failed\n");
    return 0;
  }
  complete = (char*)malloc(sizeof(char)*size);
  if (complete == 0)
  {
    fprintf(stderr,"ERROR: malloc for complete failed\n");
    return 0;
  }
  original = (int*)malloc(sizeof(int)*size);
  if (original == 0)
  {
    fprintf(stderr,"ERROR: malloc for original failed\n");
    return 0;
  }
  next_in_outlist = (int*)malloc(sizeof(int)*size);
  if (next_in_outlist == 0)
  {
    fprintf(stderr,"ERROR: malloc for next_in_outlist failed\n");
    return 0;
  }
  prev_in_outlist = (int*)malloc(sizeof(int)*size);
  if (prev_in_outlist == 0)
  {
    fprintf(stderr,"ERROR: malloc for prev_in_outlist failed\n");
    return 0;
  }
  if (USE_EDATA)
  {
    buffer_edata = (const void**)malloc(sizeof(const void*)*size*3);
    if (buffer_edata == 0)
    {
      fprintf(stderr,"ERROR: malloc for buffer_edata failed\n");
      return 0;
    }
  }
  else
  {
    buffer_edata = 0;
  }
  for (int i = 0; i < size; i++)
  {
    twinorigin[3*i] = 3*(i+1);
    complete[i] = 4;
  }
  twinorigin[3*(size-1)] = -1;

  triangle_buffer_next = 0;
  triangle_buffer_alloc = size;
  return 1;
}

int PSconverter::alloc_triangle() // return index of edge0 of the triangle
{
  if (triangle_buffer_next == -1)
  {
    twinorigin = (int*)realloc(twinorigin,sizeof(int)*2*triangle_buffer_alloc*3);
    if (twinorigin == 0)
    {
      fprintf(stderr,"ERROR: realloc for twinorigin failed\n");
      return -1;
    }
    twininv = (int*)realloc(twininv,sizeof(int)*2*triangle_buffer_alloc*3);
    if (twininv == 0)
    {
      fprintf(stderr,"ERROR: realloc for twininv failed\n");
      return -1;
    }
    complete = (char*)realloc(complete,sizeof(char)*2*triangle_buffer_alloc);
    if (complete == 0)
    {
      fprintf(stderr,"ERROR: realloc for complete failed\n");
      return -1;
    }
    original = (int*)realloc(original,sizeof(int)*2*triangle_buffer_alloc);
    if (original == 0)
    {
      fprintf(stderr,"ERROR: realloc for original failed\n");
      return -1;
    }
    next_in_outlist = (int*)realloc(next_in_outlist,sizeof(int)*2*triangle_buffer_alloc);
    if (next_in_outlist == 0)
    {
      fprintf(stderr,"ERROR: realloc for next_in_outlist failed\n");
      return -1;
    }
    prev_in_outlist = (int*)realloc(prev_in_outlist,sizeof(int)*2*triangle_buffer_alloc);
    if (prev_in_outlist == 0)
    {
      fprintf(stderr,"ERROR: realloc for prev_in_outlist failed\n");
      return -1;
    }
    if (USE_EDATA)
    {
      buffer_edata = (const void**)realloc(buffer_edata,sizeof(const void*)*2*triangle_buffer_alloc*3);
      if (buffer_edata == 0)
      {
        fprintf(stderr,"ERROR: realloc for buffer_edata failed\n");
        return -1;
      }
    }
    for (int i = triangle_buffer_alloc; i < 2*triangle_buffer_alloc; i++)
    {
      twinorigin[3*i] = 3*(i+1);
      complete[i] = 4;
    }
    twinorigin[3*(2*triangle_buffer_alloc-1)] = -1;

    triangle_buffer_next = 3*triangle_buffer_alloc;
    triangle_buffer_alloc = 2*triangle_buffer_alloc;
  }
  int te0 = triangle_buffer_next;
  triangle_buffer_next = twinorigin[te0];

#ifdef PRINT_CONTROL_OUTPUT
  triangle_buffer_size++;
  if (triangle_buffer_size > triangle_buffer_maxsize) triangle_buffer_maxsize = triangle_buffer_size;
#endif

  twininv[te0] = PS_NOT_INITIALIZED;
  twininv[te0+1] = PS_NOT_INITIALIZED;
  twininv[te0+2] = PS_NOT_INITIALIZED;
  complete[te0/3] = 0;
  return te0;
}

void PSconverter::dealloc_triangle(int te0) // takes index of edge0 of the triangle
{
  twinorigin[te0] = triangle_buffer_next;
  complete[te0/3] = 4;
  triangle_buffer_next = te0;
#ifdef PRINT_CONTROL_OUTPUT
  triangle_buffer_size--;
#endif
}

static int pscv_buffer_alloc;
static int pscv_buffer_next;
static PSconnectivityVertex* pscv_buffer;

int PSconverter::init_connectivity_vertex_buffer(int size)
{
  pscv_buffer = (PSconnectivityVertex*)malloc(sizeof(PSconnectivityVertex)*size);
  if (pscv_buffer == 0)
  {
    fprintf(stderr,"ERROR: malloc for pscv_buffer failed\n");
    return 0;
  }
  for (int i = 0; i < size; i++)
  {
    pscv_buffer[i].index = i+1;
    pscv_buffer[i].list = 0;
  }
  pscv_buffer[size-1].index = -1;

  pscv_buffer_next = 0;
  pscv_buffer_alloc = size;
  return 1;
}

int PSconverter::alloc_connectivity_vertex()
{
  if (pscv_buffer_next == -1)
  {
    pscv_buffer = (PSconnectivityVertex*)realloc(pscv_buffer,sizeof(PSconnectivityVertex)*2*pscv_buffer_alloc);
    if (pscv_buffer == 0)
    {
      fprintf(stderr,"ERROR: realloc for pscv_buffer failed\n");
      return -1;
    }
    for (int i = pscv_buffer_alloc; i < 2*pscv_buffer_alloc; i++)
    {
      pscv_buffer[i].index = i+1;
      pscv_buffer[i].list = 0;
    }
    pscv_buffer[2*pscv_buffer_alloc-1].index = -1;
    
    pscv_buffer_next = pscv_buffer_alloc;
    pscv_buffer_alloc = 2*pscv_buffer_alloc;
  }
  // get index of next available vertex
  int pscv_idx = pscv_buffer_next;
  pscv_buffer_next = pscv_buffer[pscv_idx].index;
  if (pscv_buffer[pscv_idx].list == 0)
  {
    pscv_buffer[pscv_idx].list = (int*)malloc(sizeof(int)*10);
    pscv_buffer[pscv_idx].list_alloc = 10;
  }
  pscv_buffer[pscv_idx].list_size = 0;

#ifdef PRINT_CONTROL_OUTPUT
  pscv_buffer_size++; if (pscv_buffer_size > pscv_buffer_maxsize) pscv_buffer_maxsize = pscv_buffer_size;
#endif

  return pscv_idx;
}

void PSconverter::dealloc_connectivity_vertex(int pscv_idx)
{
  pscv_buffer[pscv_idx].index = pscv_buffer_next;
  pscv_buffer_next = pscv_idx;
#ifdef PRINT_CONTROL_OUTPUT
  pscv_buffer_size--;
#endif
}

static int psov_buffer_alloc;
static int psov_buffer_next;
static PSoutputVertex* psov_buffer;

int PSconverter::init_output_vertex_buffer(int size)
{
  psov_buffer = (PSoutputVertex*)malloc(sizeof(PSoutputVertex)*size);
  if (psov_buffer == 0)
  {
    fprintf(stderr,"ERROR: malloc for psov_buffer failed\n");
    return 0;
  }
  for (int i = 0; i < size; i++)
  {
    psov_buffer[i].index = i+1;
  }
  psov_buffer[size-1].index = -1;

  psov_buffer_next = 0;
  psov_buffer_alloc = size;

  return 1;
}

int PSconverter::alloc_output_vertex()
{
  if (psov_buffer_next == -1)
  {
    psov_buffer = (PSoutputVertex*)realloc(psov_buffer,sizeof(PSoutputVertex)*2*psov_buffer_alloc);
    if (psov_buffer == 0)
    {
      fprintf(stderr,"realloc for psov_buffer failed\n");
      return -1;
    }
    for (int i = psov_buffer_alloc; i < 2*psov_buffer_alloc; i++)
    {
      psov_buffer[i].index = i+1;
    }
    psov_buffer[2*psov_buffer_alloc-1].index = -1;

    psov_buffer_next = psov_buffer_alloc;
    psov_buffer_alloc = 2*psov_buffer_alloc;
  }
  // get index of next available vertex
  int psov_idx = psov_buffer_next;
  psov_buffer_next = psov_buffer[psov_idx].index;
  psov_buffer[psov_idx].index = -1;
  psov_buffer[psov_idx].use_count = 0;
  psov_buffer[psov_idx].non_manifold = -1;

#ifdef PRINT_CONTROL_OUTPUT
  psov_buffer_size++; if (psov_buffer_size > psov_buffer_maxsize) psov_buffer_maxsize = psov_buffer_size;
#endif

  return psov_idx;
}

void PSconverter::dealloc_output_vertex(int psov_idx)
{
  psov_buffer[psov_idx].index = psov_buffer_next;
  psov_buffer_next = psov_idx;
#ifdef PRINT_CONTROL_OUTPUT
  psov_buffer_size--;
#endif
}

static int* sort_edge_list;
static int sort_edge_list_alloced;

static void initSortEdgeList(int size)
{
  sort_edge_list_alloced = size;
  sort_edge_list = (int*)malloc(sizeof(int)*sort_edge_list_alloced);
}

///// functions to traverse the twinedge structure

#define MASK 0x5FFFFFFF
#define MARK 0x80000000

// get the next edge
static inline int next(int e)
{
  return (3 * (e / 3) + ((e + 1) % 3));
}

// get the prev edge
static inline int prev(int e)
{
  return (3 * (e / 3) + ((e + 2) % 3));
}

// get the inverse edge
static inline int inv(int e)
{
  return (twininv[e]);
}

static inline void setinv(int e, int i)
{
  twininv[e] = i;
}

// get the origin of the edge

static inline int origin(int e)
{
  return (twinorigin[e]);
}

static void quicksort_sort_edge_list(int* a, int i, int j)
{
  int in_i = i;
  int in_j = j;
  int w;
  int x = origin(a[(i+j)/2] & MASK);
  do
  {
    while (origin(a[i] & MASK) > x) i++;
    while (origin(a[j] & MASK) < x) j--;
    if (i<j)
    {
      w = a[i];
      a[i] = a[j];
      a[j] = w;
    }
  } while (++i<=--j);
  if (i == j+3)
  {
    i--;
    j++;
  }
  if (j>in_i) quicksort_sort_edge_list(a, in_i, j);
  if (i<in_j) quicksort_sort_edge_list(a, i, in_j);
}

static void updateOutputTriangle(int te)
{
  int t = te/3;

  if (complete[t] != 3) // this triangle is not yet in any output list
  {
    // yet one of its edges has already entered the output boundary
    twininv[te] = PS_OUTPUT_BOUNDARY_EDGE;
    return;
  }

  int te0 = 3*t;

  // how many edges of this triangle are currently on the output boundary
  int num_boundary_edges = (twininv[te0] == PS_OUTPUT_BOUNDARY_EDGE) + (twininv[te0+1] == PS_OUTPUT_BOUNDARY_EDGE) + (twininv[te0+2] == PS_OUTPUT_BOUNDARY_EDGE);

  // another of its edges enters the output boundary
  twininv[te] = PS_OUTPUT_BOUNDARY_EDGE;

  // delete a triangle from the list it is currently in and add to its new list
  if (num_boundary_edges == 0)
  {
    // delete
    if (t == outlist0) // maybe we delete the pointed to element
    {
      if (next_in_outlist[t] == t) // this could be the only element
      {
        outlist0 = -1;
      }
      else
      {
        outlist0 = next_in_outlist[t];
        next_in_outlist[prev_in_outlist[t]] = outlist0;
        prev_in_outlist[outlist0] = prev_in_outlist[t];
      }
    }
    else
    {
      next_in_outlist[prev_in_outlist[t]] = next_in_outlist[t];
      prev_in_outlist[next_in_outlist[t]] = prev_in_outlist[t];
    }
    outlist0_available--;
    // add
    if (outlist1 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist1 = t;
    }
    else
    {
      next_in_outlist[t] = outlist1;
      prev_in_outlist[t] = prev_in_outlist[outlist1];
      next_in_outlist[prev_in_outlist[outlist1]] = t;
      prev_in_outlist[outlist1] = t;
    }
    outlist1_available++;
  }
  else if (num_boundary_edges == 1)
  {
    // delete
    if (t == outlist1) // maybe we delete the pointed to element
    {
      if (next_in_outlist[t] == t) // this could be the only element
      {
        outlist1 = -1;
      }
      else
      {
        outlist1 = next_in_outlist[t];
        next_in_outlist[prev_in_outlist[t]] = outlist1;
        prev_in_outlist[outlist1] = prev_in_outlist[t];
      }
    }
    else
    {
      next_in_outlist[prev_in_outlist[t]] = next_in_outlist[t];
      prev_in_outlist[next_in_outlist[t]] = prev_in_outlist[t];
    }
    outlist1_available--;
    // add
    if (outlist2 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist2 = t;
    }
    else
    {
      next_in_outlist[t] = outlist2;
      prev_in_outlist[t] = prev_in_outlist[outlist2];
      next_in_outlist[prev_in_outlist[outlist2]] = t;
      prev_in_outlist[outlist2] = t;
    }
    outlist2_available++;
  }
  else
  {
    // delete
    if (t == outlist2) // maybe we delete the pointed to element
    {
      if (next_in_outlist[t] == t) // this could be the only element
      {
        outlist2 = -1;
      }
      else
      {
        outlist2 = next_in_outlist[t];
        next_in_outlist[prev_in_outlist[t]] = outlist2;
        prev_in_outlist[outlist2] = prev_in_outlist[t];
      }
    }
    else
    {
      next_in_outlist[prev_in_outlist[t]] = next_in_outlist[t];
      prev_in_outlist[next_in_outlist[t]] = prev_in_outlist[t];
    }
    outlist2_available--;
    // add
    if (outlist3 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist3 = t;
    }
    else
    {
      next_in_outlist[t] = outlist3;
      prev_in_outlist[t] = prev_in_outlist[outlist3];
      next_in_outlist[prev_in_outlist[outlist3]] = t;
      prev_in_outlist[outlist3] = t;
    }
    outlist3_available++;
  }
}

static void addOutputTriangle(int t)
{
  int te0 = 3*t;
  int num_boundary_edges = (twininv[te0] == PS_OUTPUT_BOUNDARY_EDGE) + (twininv[te0+1] == PS_OUTPUT_BOUNDARY_EDGE) + (twininv[te0+2] == PS_OUTPUT_BOUNDARY_EDGE);

  if (num_boundary_edges == 0)
  {
    if (outlist0 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist0 = t;
    }
    else
    {
      next_in_outlist[t] = outlist0;
      prev_in_outlist[t] = prev_in_outlist[outlist0];
      next_in_outlist[prev_in_outlist[outlist0]] = t;
      prev_in_outlist[outlist0] = t;
    }
    outlist0_available++;
  }
  else if (num_boundary_edges == 1)
  {
    if (outlist1 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist1 = t;
    }
    else
    {
      next_in_outlist[t] = outlist1;
      prev_in_outlist[t] = prev_in_outlist[outlist1];
      next_in_outlist[prev_in_outlist[outlist1]] = t;
      prev_in_outlist[outlist1] = t;
    }
    outlist1_available++;
  }
  else if (num_boundary_edges == 2)
  {
    if (outlist2 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist2 = t;
    }
    else
    {
      next_in_outlist[t] = outlist2;
      prev_in_outlist[t] = prev_in_outlist[outlist2];
      next_in_outlist[prev_in_outlist[outlist2]] = t;
      prev_in_outlist[outlist2] = t;
    }
    outlist2_available++;
  }
  else
  {
    if (outlist3 == -1)
    {
      next_in_outlist[t] = t;
      prev_in_outlist[t] = t;
      outlist3 = t;
    }
    else
    {
      next_in_outlist[t] = outlist3;
      prev_in_outlist[t] = prev_in_outlist[outlist3];
      next_in_outlist[prev_in_outlist[outlist3]] = t;
      prev_in_outlist[outlist3] = t;
    }
    outlist3_available++;
  }
  output_triangles_available++;
}

static int vdata[3];
static int edata[3];

void PSconverter::process_finalized_vertex(int pscv_idx)
{
  int i,j,k,l;
  int sort_edge_list_size = 0;

  PSconnectivityVertex* pscv = &(pscv_buffer[pscv_idx]);

  if (sort_edge_list_alloced < 2*pscv->list_size)
  {
    sort_edge_list_alloced = 2*pscv->list_size;
    free(sort_edge_list);
    sort_edge_list = (int*)malloc(sizeof(int)*sort_edge_list_alloced);
  }

  for (i = 0; i < pscv->list_size; i++)
  {
    j = pscv->list[i];
    k = j/3;
    complete[k]++;
    next_in_outlist[k] = -1; // mark triangle not visited (reuse of this data field for efficiency reasons)
    if (complete[k] == 3)
    {
#ifdef PRINT_CONTROL_OUTPUT
      k = next(j);
      if (inv(j) == PS_NOT_INITIALIZED) fprintf(stderr,"error\n");
      k = prev(j);
      if (inv(k) == PS_NOT_INITIALIZED) fprintf(stderr,"error\n");
#endif
    }
    else
    {
      k = next(j);
      if (inv(j) == PS_NOT_INITIALIZED) sort_edge_list[sort_edge_list_size++] = k;
      k = prev(j);
      if (inv(k) == PS_NOT_INITIALIZED) sort_edge_list[sort_edge_list_size++] = k | MARK;
    }
  }

  if (sort_edge_list_size)
  {
    quicksort_sort_edge_list(sort_edge_list,0,sort_edge_list_size-1);
    i = 0;
    while (i < sort_edge_list_size)
    {
      j = i+1;
      k = j+1;
      if (j == sort_edge_list_size || origin(sort_edge_list[i]&MASK) != origin(sort_edge_list[j]&MASK)) // border
      {
#ifdef PRINT_CONTROL_OUTPUT
        number_border_edges++;
#endif
        if (sort_edge_list[i]&MARK)
        {
          setinv(sort_edge_list[i]&MASK,PS_BORDER_EDGE);
        }
        else
        {
          setinv(prev(sort_edge_list[i]),PS_BORDER_EDGE);
        }
        i = j;
      }
      else
      {
        if (k == sort_edge_list_size || (origin(sort_edge_list[i]&MASK) != origin(sort_edge_list[k]&MASK)))
        {
          if ((sort_edge_list[i] & MARK) ^ (sort_edge_list[j] & MARK)) // manifold and oriented edges
          {
#ifdef PRINT_CONTROL_OUTPUT
            number_manifold_edges++;
#endif
            if (sort_edge_list[i]&MARK)
            {
              setinv(sort_edge_list[i]&MASK,prev(sort_edge_list[j]));
              setinv(prev(sort_edge_list[j]),sort_edge_list[i]&MASK);
            }
            else
            {
              setinv(prev(sort_edge_list[i]),sort_edge_list[j]&MASK);
              setinv(sort_edge_list[j]&MASK,prev(sort_edge_list[i]));
            }
          }
          else // not oriented edges
          {
#ifdef PRINT_CONTROL_OUTPUT
            number_not_oriented_edges++;
#endif
            if (sort_edge_list[i]&MARK)
            {
              setinv(sort_edge_list[i]&MASK,PS_NOT_ORIENTED_EDGE);
            }
            else
            {
              setinv(prev(sort_edge_list[i]),PS_NOT_ORIENTED_EDGE);
            }
            if (sort_edge_list[j]&MARK)
            {
              setinv(sort_edge_list[j]&MASK,PS_NOT_ORIENTED_EDGE);
            }
            else
            {
              setinv(prev(sort_edge_list[j]),PS_NOT_ORIENTED_EDGE);
            }
          }
          i = k;
        }
        else // non-manifold edge
        {
#ifdef PRINT_CONTROL_OUTPUT
          number_non_manifold_edges+=2;
#endif
          if (sort_edge_list[i]&MARK)
          {
            setinv(sort_edge_list[i]&MASK,PS_NON_MANIFOLD_EDGE);
          }
          else
          {
            setinv(prev(sort_edge_list[i]),PS_NON_MANIFOLD_EDGE);
          }
          if (sort_edge_list[j]&MARK)
          {
            setinv(sort_edge_list[j]&MASK,PS_NON_MANIFOLD_EDGE);
          }
          else
          {
            setinv(prev(sort_edge_list[j]),PS_NON_MANIFOLD_EDGE);
          }
          while (k < sort_edge_list_size && origin(sort_edge_list[i]&MASK) == origin(sort_edge_list[k]&MASK))
          {
#ifdef PRINT_CONTROL_OUTPUT
            number_non_manifold_edges++;
#endif
            if (sort_edge_list[k]&MARK)
            {
              setinv(sort_edge_list[k]&MASK,PS_NON_MANIFOLD_EDGE);
            }
            else
            {
              setinv(prev(sort_edge_list[k]),PS_NON_MANIFOLD_EDGE);
            }
            k++;
          }
          i = k;
        }
      }
    }
  }

  // vertex use-count and non-manifold vertex detection
  int psov_idx = -1;
  PSoutputVertex* psov = 0;
  int psov_nm_idx = -1;
  i = 0;

  // loop over half-edges incident to pscv
  while (i < pscv->list_size)
  {
    j = pscv->list[i]; // get half-edge
    k = j/3;           // get the triangle this half-edge is part of
    if (next_in_outlist[k] == -1) // found a triangle (ring) that has not yet been processed ...
    {
      psov_nm_idx = psov_idx;
      psov_idx = alloc_output_vertex(); // create a new output vertex for it
      psov = &(psov_buffer[psov_idx]);
      psov->original = pscv->index; // original index
      psov->vflag = 0;
      psov->use_count--;
      psov->non_manifold = psov_nm_idx;
      VecCopy3fv(psov->v, pscv->v); // actual position
      twinorigin[j] = psov_idx;
      next_in_outlist[k] = 0;
      // go clock-wise
      l = inv(j);
      while(l >= 0) // while we have not hit a border/non-manifold/not-oriented edge
      {
        l = next(l);
        if (l == j)
        {
          break;
        }
        twinorigin[l] = psov_idx;
        psov->use_count--;
        next_in_outlist[l/3] = 0;
        l = inv(l);
      }
      if (l < 0)
      {
        if (l == PS_BORDER_EDGE)
        {
          psov->vflag |= PS_BORDER;
        }
        else if (l == PS_NON_MANIFOLD_EDGE)
        {
          psov->vflag |= PS_NON_MANIFOLD;
        }
        else if (l == PS_NOT_ORIENTED_EDGE)
        {
          psov->vflag |= PS_NOT_ORIENTED;
        }
      }
      // did we complete a closed triangle ring around the vertex?
      if (l != j)
      {
        // no ... so go counterclockwise
        l = inv(prev(j));
        while (l >= 0) // while we have not hit a border/non-manifold/not-oriented edge
        {
          if (l == j)
          {
            break;
          }
          twinorigin[l] = psov_idx;
          psov->use_count--;
          next_in_outlist[l/3] = 0;
          l = inv(prev(l));
        }
        if (l < 0)
        {
          if (l == PS_BORDER_EDGE)
          {
            psov->vflag |= PS_BORDER;
          }
          else if (l == PS_NON_MANIFOLD_EDGE)
          {
            psov->vflag |= PS_NON_MANIFOLD;
          }
          else if (l == PS_NOT_ORIENTED_EDGE)
          {
            psov->vflag |= PS_NOT_ORIENTED;
          }
        }
      }
    }
    if (complete[k] == 3)
    {
      addOutputTriangle(k);
    }
    i++;
  }

  if (psov_nm_idx != -1)
  {
    while (psov_buffer[psov_nm_idx].non_manifold != -1) psov_nm_idx = psov_buffer[psov_nm_idx].non_manifold;
    psov_buffer[psov_nm_idx].non_manifold = psov_idx;
  }
}

int PSconverter::process_event()
{
  int te0;
  my_pscv_hash::iterator hash_element;
  int pscv_idx;

  int event = smreader->read_event();

  if (event == SM_VERTEX)
  {
    if (smreader->post_order) // vertex should already be in hash
    {
      hash_element = pscv_hash->find(smreader->v_idx);
      if (hash_element == pscv_hash->end())
      {
        fprintf(stderr,"FATAL ERROR: pscv not found in hash (post-order traversal)\n");
        exit(1);
      }
      pscv_idx = (*hash_element).second;
    }
    else                      // vertex is created and put into hash
    {
      pscv_idx = alloc_connectivity_vertex();
      pscv_buffer[pscv_idx].index = smreader->v_idx;
      pscv_hash->insert(my_pscv_hash::value_type(smreader->v_idx, pscv_idx));
    }
    VecCopy3fv((&(pscv_buffer[pscv_idx]))->v,smreader->v_pos_f);
  }
  else if (event == SM_TRIANGLE)
  {
    te0 = alloc_triangle();
    original[te0/3] = smreader->f_count-1;

    // first vertex

    hash_element = pscv_hash->find(smreader->t_idx[0]);
    if (hash_element == pscv_hash->end())
    {
      if (smreader->post_order) // vertex is created here and put into hash
      {
        pscv_idx = alloc_connectivity_vertex();
        pscv_buffer[pscv_idx].index = smreader->t_idx[0];
        pscv_hash->insert(my_pscv_hash::value_type(smreader->t_idx[0], pscv_idx));
      }
      else
      {
        fprintf(stderr,"FATAL ERROR: first pscv not found in hash (pre-order traversal)\n");
        exit(1);
      }
    }
    else
    {
      pscv_idx = (*hash_element).second;
    }
    twinorigin[te0] = pscv_idx;
    if (pscv_buffer[pscv_idx].list_alloc == pscv_buffer[pscv_idx].list_size)
    {
      pscv_buffer[pscv_idx].list_alloc = pscv_buffer[pscv_idx].list_alloc + 2;
      pscv_buffer[pscv_idx].list = (int*)realloc(pscv_buffer[pscv_idx].list, sizeof(int)*pscv_buffer[pscv_idx].list_alloc);
    }
    pscv_buffer[pscv_idx].list[pscv_buffer[pscv_idx].list_size] = te0;
    pscv_buffer[pscv_idx].list_size++;

    // second vertex

    hash_element = pscv_hash->find(smreader->t_idx[1]);
    if (hash_element == pscv_hash->end())
    {
      if (smreader->post_order) // vertex is created here and put into hash
      {
        pscv_idx = alloc_connectivity_vertex();
        pscv_buffer[pscv_idx].index = smreader->t_idx[1];
        pscv_hash->insert(my_pscv_hash::value_type(smreader->t_idx[1], pscv_idx));
      }
      else
      {
        fprintf(stderr,"internal error: second pscv not found in hash (pre-order traversal)\n");
        exit(1);
      }
    }
    else
    {
      pscv_idx = (*hash_element).second;
    }
    twinorigin[te0+1] = pscv_idx;
    if (pscv_buffer[pscv_idx].list_alloc == pscv_buffer[pscv_idx].list_size)
    {
      pscv_buffer[pscv_idx].list_alloc = pscv_buffer[pscv_idx].list_alloc + 2;
      pscv_buffer[pscv_idx].list = (int*)realloc(pscv_buffer[pscv_idx].list, sizeof(int)*pscv_buffer[pscv_idx].list_alloc);
    }
    pscv_buffer[pscv_idx].list[pscv_buffer[pscv_idx].list_size] = te0+1;
    pscv_buffer[pscv_idx].list_size++;

    // third vertex

    hash_element = pscv_hash->find(smreader->t_idx[2]);
    if (hash_element == pscv_hash->end())
    {
      if (smreader->post_order) // vertex is created here and put into hash
      {
        pscv_idx = alloc_connectivity_vertex();
        pscv_buffer[pscv_idx].index = smreader->t_idx[2];
        pscv_hash->insert(my_pscv_hash::value_type(smreader->t_idx[2], pscv_idx));
      }
      else
      {
        fprintf(stderr,"internal error: third pscv not found in hash (pre-order traversal)\n");
        exit(1);
      }
    }
    else
    {
      pscv_idx = (*hash_element).second;
    }
    twinorigin[te0+2] = pscv_idx;
    if (pscv_buffer[pscv_idx].list_alloc == pscv_buffer[pscv_idx].list_size)
    {
      pscv_buffer[pscv_idx].list_alloc = pscv_buffer[pscv_idx].list_alloc + 2;
      pscv_buffer[pscv_idx].list = (int*)realloc(pscv_buffer[pscv_idx].list, sizeof(int)*pscv_buffer[pscv_idx].list_alloc);
    }
    pscv_buffer[pscv_idx].list[pscv_buffer[pscv_idx].list_size] = te0+2;
    pscv_buffer[pscv_idx].list_size++;
  }
  else if (event == SM_FINALIZED)
  {
    hash_element = pscv_hash->find(smreader->final_idx);
    if (hash_element == pscv_hash->end())
    {
      fprintf(stderr,"FATAL error: explicitely finalized pscv not found in hash\n");
      exit(1);
    }
    pscv_idx = (*hash_element).second;
    process_finalized_vertex(pscv_idx);
    pscv_hash->erase(hash_element);
    dealloc_connectivity_vertex(pscv_idx);
  }
  else if (event == SM_EOF)
  {
    if (pscv_hash->empty())
    {
      // the streaming mesh is completely read *and* there are no unfinalized
      // vertices in the hash. we are done.
      return 0;
    }
    else
    {
      // in case there are vertices that have not yet been finalized we have
      // to make one pass over the entire hash to finalize them in the order
      // that they came streaming in
      hash_element = pscv_hash->find(pscv_completed);
      pscv_completed++;
      if (hash_element == pscv_hash->end())
      {
        fprintf(stderr,"FATAL ERROR: remaining pscv not found in hash\n");
        exit(1);
      }
      else
      {
        pscv_idx = (*hash_element).second;
        process_finalized_vertex(pscv_idx);
        pscv_hash->erase(hash_element);
        dealloc_connectivity_vertex(pscv_idx);
      }
    }
  }
  else
  {
    fprintf(stderr,"WARNING: unknown streaming mesh event\n");
  }

  return 1;
}

void PSconverter::fill_output_buffer()
{
  while (output_triangles_available < output_triangles_buffer_high)
  {
    if (process_event() == 0)
    {
      output_triangles_buffer_low = -1;
      output_triangles_buffer_high = -1;
    }
  }
}

bool PSconverter::open(SMreader* smr, int low, int high)
{
  smreader = smr; 
  output_triangles_buffer_low = low;
  output_triangles_buffer_high = high;

  // copy processing sequence stats from streaming mesh

  nverts = smreader->nverts;
  nfaces = smreader->nfaces;

  bb_min_f = smreader->bb_min_f;
  bb_max_f = smreader->bb_max_f;

  // init counters / state

  v_count = 0;
  f_count = 0;
  h_count = -1;  // not supported
  c_count = -1;  // not supported

  nm_v_count = 0;
  nm_e_count = 0;

  init_connectivity_vertex_buffer(1024);
  init_output_vertex_buffer(1024);
  init_triangle_buffer(2048);
  initSortEdgeList(30);

  pscv_hash = new my_pscv_hash;

  outlist0 = -1;
  outlist1 = -1;
  outlist2 = -1;
  outlist3 = -1;

  output_triangles_available = 0;

  outlist0_available = 0;
  outlist1_available = 0;
  outlist2_available = 0;
  outlist3_available = 0;

  pscv_completed = 0;

#ifdef PRINT_CONTROL_OUTPUT
  output_type0 = 0;
  output_type1 = 0;
  output_type2 = 0;
  output_type3 = 0;

  pscv_buffer_size = 0;
  psov_buffer_size = 0;
  triangle_buffer_size = 0;

  pscv_buffer_maxsize = 0;
  psov_buffer_maxsize = 0;
  triangle_buffer_maxsize = 0;

  number_border_edges = 0;
  number_manifold_edges = 0;
  number_non_manifold_edges = 0;
  number_not_oriented_edges = 0;

  fprintf(stderr,"starting sequence ...\n");
#endif

  fill_output_buffer();

  return true;
}

void PSconverter::close()
{
#ifdef PRINT_CONTROL_OUTPUT
  fprintf(stderr, "done.\n");

  fprintf(stderr,"connectivity vertex buffer: alloc %d maxsize %d size %d\n",pscv_buffer_alloc, pscv_buffer_maxsize,pscv_buffer_size);
  fprintf(stderr,"output vertex buffer: alloc %d maxsize %d size %d\n",psov_buffer_alloc,psov_buffer_maxsize,psov_buffer_size);
  fprintf(stderr,"triangle buffer: alloc %d maxsize %d\n",triangle_buffer_alloc,triangle_buffer_maxsize);

  fprintf(stderr,"half-edges: border %d manifold %d not-oriented %d non-manifold %d\n",number_border_edges,number_manifold_edges*2,number_not_oriented_edges*2,number_non_manifold_edges);
  fprintf(stderr,"%d %d %d %d %d\n",output_triangles_available,outlist3_available,outlist2_available,outlist1_available,outlist0_available);
  fprintf(stderr,"start %d add+join %d fill %d end %d\n",output_type0,output_type1,output_type2,output_type3);
#endif

  free(sort_edge_list);

  delete pscv_hash;

  if (nverts != -1 && nverts != v_count)
  {
    fprintf(stderr,"WARNING: wrong vertex count: v_count (%d) != nverts (%d)\n", v_count, nverts);
  }
  nverts = v_count;
  
  if (nfaces != -1 && nfaces != f_count)
  {
    fprintf(stderr,"WARNING: wrong face count: f_count (%d) != nfaces (%d)\n", f_count, nfaces);
  }
  nfaces = f_count;
}

PSevent PSconverter::read_triangle()
{
  int i, t, te0, psov_idx, psov_nm_idx;
  PSoutputVertex* psov;

  if (output_triangles_available < output_triangles_buffer_low)
  {
    fill_output_buffer();
  }

#ifdef PRINT_CONTROL_OUTPUT
  if (output_triangles_available != outlist3_available+outlist2_available+outlist1_available+outlist0_available)
  {
    fprintf(stderr,"FATAL ERROR: output_triangles_available != outlist3_available+outlist2_available+outlist1_available+outlist0_available\n");
  }
#endif

  if (!output_triangles_available)
  {
    return PS_EOF;
  }

  vdata[0] = -1;
  vdata[1] = -1;
  vdata[2] = -1;

  edata[0] = -1;
  edata[1] = -1;
  edata[2] = -1;

  if (outlist3 != -1) // try get triangle from outlist3 (-> 'end' operation)
  {
    t = outlist3;
    te0 = 3*t;
    if (next_in_outlist[t] == t) // delete triangle from outlist3
    {
      outlist3 = -1;
    }
    else
    {
      outlist3 = next_in_outlist[t];
      next_in_outlist[prev_in_outlist[t]] = outlist3;
      prev_in_outlist[outlist3] = prev_in_outlist[t];
    }
    outlist3_available--;
#ifdef PRINT_CONTROL_OUTPUT
    output_type3++;
#endif
  }
  else if (outlist2 != -1) // try get triangle from outlist2 (-> 'fill' operation)
  {
    t = outlist2;
    te0 = 3*t;
    if (next_in_outlist[t] == t) // delete triangle from outlist2
    {
      outlist2 = -1;
    }
    else
    {
      outlist2 = next_in_outlist[t];
      next_in_outlist[prev_in_outlist[t]] = outlist2;
      prev_in_outlist[outlist2] = prev_in_outlist[t];
    }
    outlist2_available--;
#ifdef PRINT_CONTROL_OUTPUT
    output_type2++;
#endif
  }
  else if (outlist1 != -1) // try get triangle from outlist1 (-> 'add/split' operations)
  {
    t = outlist1;
    te0 = 3*t;
    if (next_in_outlist[t] == t) // delete triangle from outlist1
    {
      outlist1 = -1;
    }
    else
    {
      outlist1 = next_in_outlist[t];
      next_in_outlist[prev_in_outlist[t]] = outlist1;
      prev_in_outlist[outlist1] = prev_in_outlist[t];
    }
    outlist1_available--;
#ifdef PRINT_CONTROL_OUTPUT
    output_type1++;
#endif
  }
  else  // try get triangle from outlist0 (-> 'start' operations)
  {
    t = outlist0;
    te0 = 3*t;
    // skip triangles that are vertex-adjacent (-> not allowed in a Processing Sequence order)
    // this  can lead to failure when all triangles in the buffer are vertex-adjacent start triangles
    psov_idx = t; // using this hack here we check for this case
    while ((psov_buffer[twinorigin[te0]].use_count > 0) || (psov_buffer[twinorigin[te0+1]].use_count > 0) || (psov_buffer[twinorigin[te0+2]].use_count > 0))
    {
      outlist0 = next_in_outlist[outlist0];
      if (outlist0 == psov_idx)
      {
        fprintf(stderr,"FATAL ERROR: the PSconverter fails because the output triangle buffer is full\n");
        fprintf(stderr,"with %d vertex-adjacent start triangles. increase the lower buffer limit.\n",output_triangles_available);
        exit(1);
      }
      t = outlist0;
      te0 = 3*t;
    }
    if (next_in_outlist[t] == t) // delete triangle from outlist0
    {
      outlist0 = -1;
    }
    else
    {
      outlist0 = next_in_outlist[t];
      next_in_outlist[prev_in_outlist[t]] = outlist0;
      prev_in_outlist[outlist0] = prev_in_outlist[t];
    }
    outlist0_available--;
#ifdef PRINT_CONTROL_OUTPUT
    output_type0++;
#endif
//    fprintf(stderr,"using type 0: %d %d %d %d\n",outlist3_available,outlist2_available,outlist1_available,outlist0_available);
  }

  t_orig = original[t];

  // propagate boundary and set flags and user_data
  for (i = 0; i < 3; i++)
  {
    if (twininv[te0+i] >= 0)
    {
      updateOutputTriangle(twininv[te0+i]);
      t_eflag[i] = PS_FIRST; // entering_edge
      edata[i] = twininv[te0+i];
    }
    else
    {
      if (twininv[te0+i] == PS_OUTPUT_BOUNDARY_EDGE)
      {
        t_eflag[i] =  PS_LAST; // leaving_edge
        edata[i] = te0+i;
      }
      else if (twininv[te0+i] == PS_BORDER_EDGE)
      {
        t_eflag[i] = PS_FIRST | PS_LAST | PS_BORDER; // border_edge
      }
      else if (twininv[te0+i] == PS_NON_MANIFOLD_EDGE)
      {
        // ***NOTE*** PS_NON_MANIFOLD_EDGE for now implemented as PS_NON_MANIFOLD marked PS_BORDER
        t_eflag[i] = PS_FIRST | PS_LAST | PS_BORDER | PS_NON_MANIFOLD; // non_manifold_edge
      }
      else
      {
        // ***NOTE*** PS_NOT_ORIENTED_EDGE for now implemented as PS_NOT_ORIENTED marked PS_BORDER
        t_eflag[i] = PS_FIRST | PS_LAST | PS_BORDER | PS_NOT_ORIENTED; // not_oriented_edge
      }
    }
  }

  // assign vertex indices, update vertex usage, and set flags and user_data
  for (i = 0; i < 3; i++)
  {
    psov_idx = twinorigin[te0+i];
    psov = &(psov_buffer[psov_idx]);
    vdata[i] = psov_idx;
    t_vflag[i] = psov->vflag;
    if (psov->index == -1) // new vertex
    {
      if (psov->non_manifold != -1) // is it a non-manifold vertex?
      {
        psov_nm_idx = psov->non_manifold;
        while (psov_nm_idx != psov_idx)
        {
          psov_buffer[psov_nm_idx].index = v_count;
          psov_nm_idx = psov_buffer[psov_nm_idx].non_manifold;
        }
      }
      psov->index = v_count++;
      psov->use_count = -psov->use_count;
      t_vflag[i] |= PS_FIRST; 
    }
    else if (psov->use_count < 0) // here comes new instance of non-manifold vertex
    {
      psov->use_count = -psov->use_count;
      t_vflag[i] |= PS_RING_START; 
    }
    t_idx[i] = psov->index;
    t_idx_orig[i] = psov->original;
    t_pos_f[i] = psov->v;
    psov->use_count--;
    if (psov->use_count == 0) // finalized vertex
    {
      if (psov->non_manifold != -1) // then only this instance of a non_manifold vertex was finalized
      {
        psov_nm_idx = psov->non_manifold;
        while (psov_buffer[psov_nm_idx].non_manifold != psov_idx) psov_nm_idx = psov_buffer[psov_nm_idx].non_manifold;
        psov_buffer[psov_nm_idx].non_manifold = psov->non_manifold;
        if (psov_buffer[psov_nm_idx].non_manifold == psov_nm_idx)
        {
          psov_buffer[psov_nm_idx].non_manifold = -1;
        }
        t_vflag[i] |= PS_RING_END; 
      }
      else
      {
        t_vflag[i] |= PS_LAST;
      }
      dealloc_output_vertex(psov_idx);
    }
  }

  dealloc_triangle(te0);

  output_triangles_available--;

  f_count++;

  return PS_TRIANGLE;
}

void PSconverter::set_vdata(const void* data, int i)
{
  if (USE_VDATA)
  {
    if (vdata[i] == -1)
    {
      fprintf(stderr,"ERROR: you cannot set vdata %d now\n",i);
      return;
    }
    psov_buffer[vdata[i]].user_data = data;
    // is this vertex non-manifold 
    if (psov_buffer[vdata[i]].non_manifold != -1)
    {
      // yes ... but maybe this vertex ring has also just been finalized
      if (psov_buffer[vdata[i]].use_count == 0)
      {
        // yes ... so we need some special handling for the remaining vertex copies
        int psov_remaining_nm_idx = psov_buffer[vdata[i]].non_manifold;
        psov_buffer[psov_remaining_nm_idx].user_data = data;
        // and if there is more than one remaining vertex copy
        if (psov_buffer[psov_remaining_nm_idx].non_manifold != -1)
        {
          int psov_other_nm_idx = psov_buffer[psov_remaining_nm_idx].non_manifold;
          while (psov_other_nm_idx != psov_remaining_nm_idx)
          {
            psov_buffer[psov_other_nm_idx].user_data = data;
            psov_other_nm_idx = psov_buffer[psov_other_nm_idx].non_manifold;
          }
        }
      }
      else
      {
        // no ... simply set the data for all vertex copies in the non-manifold loop
        int psov_nm_idx = psov_buffer[vdata[i]].non_manifold;
        while (psov_nm_idx != vdata[i])
        {
          psov_buffer[psov_nm_idx].user_data = data;
          psov_nm_idx = psov_buffer[psov_nm_idx].non_manifold;
        }
      }
    }
  }
  else
  {
    fprintf(stderr,"error: you inititalized PSconverter to work without vdata support\n");
  }
}

void* PSconverter::get_vdata(int i) const
{
  if (USE_VDATA)
  {
    if (vdata[i] == -1)
    {
      fprintf(stderr,"you cannot get vdata %d now\n",i);
      return 0;
    }
    return (void*)psov_buffer[vdata[i]].user_data;
  }
  else
  {
    fprintf(stderr,"error: you inititalized PSconverter to work without vdata support\n");
    return 0;
  }
}

void PSconverter::set_edata(const void* data, int i)
{
  if (USE_EDATA)
  {
    if (edata[i] == -1)
    {
      fprintf(stderr,"you cannot set edata %d now\n",i);
      return;
    }
    buffer_edata[edata[i]] = data;
  }
  else
  {
    fprintf(stderr,"error: you inititalized PSconverter to work without edata support\n");
  }
}

void* PSconverter::get_edata(int i) const
{
  if (USE_EDATA)
  {
    if (edata[i] == -1)
    {
      fprintf(stderr,"you cannot get edata %d now\n",i);
      return 0;
    }
    return (void*)buffer_edata[edata[i]];
  }
  else
  {
    fprintf(stderr,"error: you inititalized PSconverter to work without edata support\n");
    return 0;
  }
}

PSconverter::PSconverter()
{
  int i;

  v_idx = PS_UNDEFINED;
  t_orig = PS_UNDEFINED;

  t_idx_orig = new int[3];

  for (i = 0; i < 3; i++)
  {
    t_idx[i] = PS_UNDEFINED;
    t_idx_orig[i] = PS_UNDEFINED;
    t_pos_f[i] = 0;
    t_vflag[i] = PS_UNDEFINED;
    t_eflag[i] = PS_UNDEFINED;
  }

  nverts = -1;
  nfaces = -1;

  v_count = 0;
  f_count = 0;

  bb_min_f = 0;
  bb_max_f = 0;
}

PSconverter::~PSconverter()
{
  delete [] t_idx_orig;
}
