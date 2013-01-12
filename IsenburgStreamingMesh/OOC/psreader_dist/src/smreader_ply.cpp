/*
===============================================================================

  FILE:  SMreader_ply.cpp
  
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
#include "smreader_ply.h"

#include <stdlib.h>
#include <string.h>

#include "vec3fv.h"
#include "vec3iv.h"

#include "ply.h"

/* vertex and face definitions for a polygonal object */

typedef struct Vertex {
  float x,y,z;
} Vertex;

typedef struct Face {
  unsigned char nverts;
  int *verts;
} Face;

static PlyProperty vertex_props[] = { /* list of property information for a vertex */
  {"x", Float32, Float32, offsetof(Vertex,x), 0, 0, 0, 0},
  {"y", Float32, Float32, offsetof(Vertex,y), 0, 0, 0, 0},
  {"z", Float32, Float32, offsetof(Vertex,z), 0, 0, 0, 0},
};

static PlyProperty face_props[] = { /* list of property information for a face */
  {"vertex_indices", Int32, Int32, offsetof(Face,verts), 1, Uint8, Uint8, offsetof(Face,nverts)},
};

static PlyFile* in_ply;

bool SMreader_ply::open(FILE* file, bool compute_bounding_box, bool skip_vertices)
{
  int i;
  int elem_count;
  char *elem_name;
  
  if (file == 0)
  {
    fprintf(stderr, "FATAL ERROR: file pointer is zero\n");
    return false;
  }

  in_ply = read_ply(file);

  if (in_ply == 0)
  {
    fprintf(stderr, "FATAL ERROR: input PLY file is corrupt\n");
    return false;
  }

  nverts = 0;
  nfaces = 0;

  for (i = 0; i < in_ply->num_elem_types; i++)
  {
    /* prepare to read the i'th list of elements */
    elem_name = setup_element_read_ply(in_ply, i, &elem_count);

		if (equal_strings ("vertex", elem_name))
		{
  		nverts = elem_count;
      velem = i;

      if (compute_bounding_box)
      {
        fprintf(stderr, "computing bounding box ...\n");

        setup_property_ply (in_ply, &vertex_props[0]);
			  setup_property_ply (in_ply, &vertex_props[1]);
			  setup_property_ply (in_ply, &vertex_props[2]);

        // alloc bounding box
        if (bb_min_f == 0) bb_min_f = new float[3];
        if (bb_max_f == 0) bb_max_f = new float[3];
        // get current file pointer position
        long here = ftell(in_ply->fp);
        // read first element
  		  get_element_ply (in_ply, (void *)v_pos_f);
        // init bounding box
        VecCopy3fv(bb_min_f, v_pos_f);
        VecCopy3fv(bb_max_f, v_pos_f);
        // process remaining elements
        for (elem_count = 1; elem_count < nverts; elem_count++)
        {
          // read an element
  		    get_element_ply (in_ply, (void *)v_pos_f);
          // update bounding box
          VecUpdateMinMax3fv(bb_min_f, bb_max_f, v_pos_f);
        }
        fprintf(stderr, "bb_min_f[0] = %ff; bb_min_f[1] = %ff; bb_min_f[2] = %ff;\n", bb_min_f[0], bb_min_f[1], bb_min_f[2]);
        fprintf(stderr, "bb_max_f[0] = %ff; bb_max_f[1] = %ff; bb_max_f[2] = %ff;\n", bb_max_f[0], bb_max_f[1], bb_max_f[2]);
        if (skip_vertices)
        {
          v_count = nverts;
        }
        else
        {
          // move file pointer back to beginning of vertex block
          fseek(in_ply->fp, here, SEEK_SET);
          v_count = 0;
        }
      }
      else if (skip_vertices)
      {
        fprintf(stderr, "skipping vertices ...\n");
        /* this is a quick & dirty hack for advancing the file pointer past the vertices */
      
			  setup_property_ply (in_ply, &vertex_props[0]);
			  setup_property_ply (in_ply, &vertex_props[1]);
			  setup_property_ply (in_ply, &vertex_props[2]);

        // get current file pointer position
        long here = ftell(in_ply->fp);
        // read one element
  		  get_element_ply (in_ply, (void *)v_pos_f);
        // get difference in file pointer position
        long size = (int)(ftell(in_ply->fp) - here);
        // move file pointer to end of vertex block
        fseek(in_ply->fp, size*(elem_count-1), SEEK_CUR);
        v_count = nverts;
      }
      else
      {
        v_count = 0;
      }
    }
		else if (equal_strings ("face", elem_name))
		{
      nfaces = elem_count;
      felem = i;
      f_count = 0;
    }
    else
    {
      fprintf(stderr, "WARNING: unknown element '%s' in PLY file\n", elem_name);
    }
  }
  
  if (nverts == 0)
  {
    fprintf(stderr, "WARNING: no vertices in PLY file\n");
  }

  if (nfaces == 0)
  {
    fprintf(stderr, "WARNING: no vertices in PLY file\n");
  }

  v_idx = -1;
  final_idx = -1;
  for (i = 0; i < 3; i++)
  {
    v_pos_f[i] = 0.0f;
    t_idx[i] = -1;
    t_final[i] = false;
  }
  return true;
}

void SMreader_ply::close()
{
  if (comments)
  {
    for (int i = 0; i < ncomments; i++)
    {
      free(comments[i]);
    }
    free(comments);
    ncomments = 0;
    comments = 0;
  }

  nverts = -1;
  nfaces = -1;

  v_count = -1;
  f_count = -1;

  close_ply (in_ply);
  free_ply (in_ply);
}

SMevent SMreader_ply::read_element()
{
  int elem_count;

  if (v_count < nverts)
  {
    if (v_count == 0)
    {
      /* prepare to read the vertex elements */
      setup_element_read_ply(in_ply, velem, &elem_count);

      /* set up for getting vertex elements */
			setup_property_ply (in_ply, &vertex_props[0]);
			setup_property_ply (in_ply, &vertex_props[1]);
			setup_property_ply (in_ply, &vertex_props[2]);
    }
		get_element_ply(in_ply, (void *)v_pos_f);
    v_idx = v_count;
    v_count++;
    return SM_VERTEX;
  }
  else if (f_count < nfaces)
  {
    if (f_count == 0)
    {
      /* prepare to read the face elements */
      setup_element_read_ply(in_ply, felem, &elem_count);

      /* set up for getting face elements */
      setup_property_ply (in_ply, &face_props[0]);
    }
		Face* face = (Face *) malloc (sizeof (Face));
    get_element_ply (in_ply, (void *) face);
    VecCopy3iv(t_idx, face->verts);
    free(face->verts);
    free(face);
    f_count++;
    return SM_TRIANGLE;
  }
  else
  {
    return SM_EOF;
  }
}

SMevent SMreader_ply::read_event()
{
  return read_element();
}

SMreader_ply::SMreader_ply()
{
  // init of SMreader interface
  ncomments = 0;
  comments = 0;

  nfaces = -1;
  nverts = -1;

  f_count = -1;
  v_count = -1;

  bb_min_f = 0;
  bb_max_f = 0;

  post_order = false;

  // init of SMreader_ply
  velem = -1;
  felem = -1;
}

SMreader_ply::~SMreader_ply()
{
  if (comments)
  {
    for (int i = 0; i < ncomments; i++)
    {
      free(comments[i]);
    }
    free(comments);
  }

  if (bb_min_f) delete [] bb_min_f;
  if (bb_max_f) delete [] bb_max_f;
}
