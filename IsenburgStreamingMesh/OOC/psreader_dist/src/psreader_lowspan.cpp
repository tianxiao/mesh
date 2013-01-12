#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ply.h"
#include "positionquantizer.h"
#include "codec_new_dec.h"
#include "vec3iv.h"
#include "vec3fv.h"
#include "vector.h"
#include "crazyvector_for_pointers.h"
#include "psreader_lowspan.h"

//#define PRINT_CONTROL_OUTPUT fprintf
#define PRINT_CONTROL_OUTPUT if (0) fprintf

#define PS_RING_START 32 // not exposed by API right now
#define PS_RING_END   64 // not exposed by API right now

#define USE_COUNT_INFLATE (1<<20)

/* vertex and face definitions for a polygonal object */

static PlyProperty boundingbox_props[] = { /* list of property information for the boundingbox data */
  {"minmax", Float32, Float32, 0, 0, 0, 0, 0},
};

/// stores user data

static const void** set_e_data[3];
static const void* get_e_data[3];

static const void** set_v_data[3];
static const void* get_v_data[3];

///// stores the quantizer

static PositionQuantizer* pq;

// structs used during decompression

typedef struct BoundaryVertex {
  int index;
  int use_count;
  const void* user_data;
  BoundaryVertex* non_manifold;
  int origin[3];
  float origin_f[3];
} BoundaryVertex;

typedef struct BoundaryEdge {
  BoundaryEdge* boundary_prev;
  BoundaryEdge* boundary_next;
  const void* user_data;
  int slots;
  int border;
  BoundaryVertex* vertex;
  int across[3];
} BoundaryEdge;

typedef struct Boundary {
  BoundaryEdge* gate;
  int length;
  int zero_slots;
  int one_slots;
  int age;
} Boundary;

// variables uses during decompression

static BoundaryEdge* gate;
static int boundary_length;
static int zero_slots;
static bool has_left_zero_slot;
static bool was_right_zero_slot;
static int one_slots;
static int queue_boundary_age;

static Boundary* boundary;

static int corr[3];
static int pred[3];

static CrazyVector* boundaryQueue;
static TSCcodec* codec;
static CrazyVector* nonmanifoldDecompress;

static BoundaryVertex* output_vertex[3];

/************************/
/**  memory management  */
/************************/

static BoundaryVertex* boundaryVertices;
static int numBoundaryVerticesAlloced;
static int maxBoundaryVerticesAlloced;

static BoundaryVertex* allocBoundaryVertex()
{
  BoundaryVertex* vertex = boundaryVertices;

  if (vertex)
  {
    boundaryVertices = vertex->non_manifold;
  }
  else
  {
    vertex = (BoundaryVertex*)malloc(sizeof(BoundaryVertex)*1024);
    for (int i = 1; i < 1023; i++)
    {
      (&vertex[i])->non_manifold = &vertex[i+1];
    }
    (&vertex[1023])->non_manifold = 0;
    boundaryVertices = &vertex[1];
  }
  numBoundaryVerticesAlloced++;
  if (numBoundaryVerticesAlloced > maxBoundaryVerticesAlloced)
  {
    maxBoundaryVerticesAlloced = numBoundaryVerticesAlloced;
  }
  vertex->non_manifold = 0;
  vertex->use_count = 0;
  return vertex;
}

static void deallocBoundaryVertex(BoundaryVertex* vertex)
{
  vertex->non_manifold = boundaryVertices;
  boundaryVertices = vertex;
  numBoundaryVerticesAlloced--;
}

static BoundaryEdge* boundaryEdges;
static int numBoundaryEdgesAlloced;
static int maxBoundaryEdgesAlloced;

static BoundaryEdge* allocBoundaryEdge()
{
  BoundaryEdge* edge = boundaryEdges;

  if (edge)
  {
    boundaryEdges = edge->boundary_next;
  }
  else
  {
    edge = (BoundaryEdge*)malloc(sizeof(BoundaryEdge)*1024);
    for (int i = 1; i < 1023; i++)
    {
      (&edge[i])->boundary_next = &edge[i+1];
    }
    (&edge[1023])->boundary_next = 0;
    boundaryEdges = &edge[1];
  }
  numBoundaryEdgesAlloced++;
  if (numBoundaryEdgesAlloced > maxBoundaryEdgesAlloced)
  {
    maxBoundaryEdgesAlloced = numBoundaryEdgesAlloced;
  }
  return edge;
}

static void deallocBoundaryEdge(BoundaryEdge* edge)
{
  edge->boundary_next = boundaryEdges;
  boundaryEdges = edge;
  numBoundaryEdgesAlloced--;
}

static Boundary* boundaries;
static int numBoundariesAlloced;
static int maxBoundariesAlloced;

static Boundary* allocBoundary(BoundaryEdge* g, int l, int zs, int os, int a)
{
  Boundary* boundary = boundaries;

  if (boundary)
  {
    boundaries = (Boundary*)boundary->gate;
  }
  else
  {
    boundary = (Boundary*)malloc(sizeof(Boundary)*64);
    for (int i = 1; i < 63; i++)
    {
      (&boundary[i])->gate = (BoundaryEdge*)(&boundary[i+1]);
    }
    (&boundary[63])->gate = 0;
    boundaries = &boundary[1];
  }
  numBoundariesAlloced++;
  if (numBoundariesAlloced > maxBoundariesAlloced)
  {
    maxBoundariesAlloced = numBoundariesAlloced;
  }

  boundary->gate = g;
  boundary->length = l;
  boundary->zero_slots = zs;
  boundary->one_slots = os;
  boundary->age = a;  

  return boundary;
}

static void deallocBoundary(Boundary* boundary)
{
  boundary->gate = (BoundaryEdge*)boundaries;
  boundaries = boundary;
  numBoundariesAlloced--;
}

static void addBoundary(Boundary* boundary)
{
  boundaryQueue->addElement(boundary);
}

static Boundary* getBoundary()
{
  Boundary* boundary = (Boundary*)boundaryQueue->firstElement();
  boundaryQueue->removeFirstElement();
  return boundary;
}

static int getQueueBoundaryAge()
{
  if (boundaryQueue->size())
  {
    // there is another boundary ... return its age
    Boundary* boundary = (Boundary*)boundaryQueue->firstElement();
    return boundary->age;
  }
  else
  {
    // there is no boundary ... return a really 'young' number
    return (1<<30);
  }
}

static int hasMoreBoundaries()
{
  return (boundaryQueue->size() != 0);
}

static Boundary* getBoundary(int i)
{
  return (Boundary*)boundaryQueue->elementAt(i);
}

static Boundary* removeBoundary(int i)
{
  Boundary* boundary = (Boundary*)boundaryQueue->elementAt(i);
  boundaryQueue->lazyRemoveElementAt(i);
  return boundary;
}

static int numberBoundaries()
{
  return boundaryQueue->size();
}

// helper function for decompression

static BoundaryEdge* next_gate(BoundaryEdge* gate)
{
  BoundaryEdge* next = gate;
  while (next->boundary_next->slots == 0)
  {
    next = next->boundary_next;
    if (next == gate)
    {
      return gate;
    }
  }
  return next;
}

static BoundaryEdge* best_gate_left_zero(BoundaryEdge* gate)
{
  BoundaryEdge* run_prev = gate;
  while (true)
  {
    if (run_prev->slots == 0)
    {
      return next_gate(run_prev);
    }
    run_prev = run_prev->boundary_prev;
  }
}

static BoundaryEdge* best_gate_right_zero(BoundaryEdge* gate)
{
  BoundaryEdge* run_next = gate;
  while (true)
  {
    if (run_next->slots == 0)
    {
      return next_gate(run_next);
    }
    run_next = run_next->boundary_next;
  }
}

static BoundaryEdge* best_gate_zero(BoundaryEdge* gate)
{
  BoundaryEdge* run_next = gate;
  BoundaryEdge* run_prev = gate->boundary_prev;
  while (true)
  {
    if (run_next->slots == 0)
    {
      return next_gate(run_next);
    }
    if (run_prev->slots == 0)
    {
      return next_gate(run_prev);
    }
    run_next = run_next->boundary_next;
    run_prev = run_prev->boundary_prev;
  }
}

static BoundaryEdge* best_gate_one(BoundaryEdge* gate)
{
  int run = 0;
  BoundaryEdge* run_next = gate;
  BoundaryEdge* run_prev = gate->boundary_prev;
  while (true)
  {
    if ((run_next->slots == 1) && (run_next->border == 0))
    {
      return run_next;
    }
    if ((run_prev->slots == 1) && (run_prev->border == 0))
    {
      return run_prev;
    }
    run++;
    if (run > ONE_SLOT_RUN) return gate;
    run_next = run_next->boundary_next;
    run_prev = run_prev->boundary_prev;
  }
}

static BoundaryEdge* best_gate(BoundaryEdge* gate, int zero_slots, bool has_left_zero_slot)
{
  if (zero_slots)
  {
    if (has_left_zero_slot)
    {
      return best_gate_left_zero(gate);
    }
    else
    {
      return best_gate_right_zero(gate);
    }
  }
  else
  {
    return gate;
  }
}

static BoundaryEdge* findOnThisBoundaryNext(BoundaryEdge* bnext, int offset, int &split_one_slots)
{
  int count = 0;
  int one = ((bnext->slots == 1) && (bnext->border == 0));
  BoundaryEdge* brun = bnext->boundary_next;
  while (count < offset)
  {
    one += ((brun->slots == 1) && (brun->border == 0));
    count++;
    brun = brun->boundary_next;
  }
  split_one_slots = one;
  return brun;
}

static BoundaryEdge* findOnThisBoundaryPrev(BoundaryEdge* bprev, int offset, int &split_one_slots)
{
  int count = 0;
  int one = 0;
  BoundaryEdge* brun = bprev->boundary_prev;
  while (count < offset)
  {
    one += ((brun->slots == 1) && (brun->border == 0));
    count++;
    brun = brun->boundary_prev;
  }
  split_one_slots = one;
  return brun;
}

static BoundaryEdge* findOnStackBoundaryNext(BoundaryEdge* bgate, int offset)
{
  int count = 0;
  BoundaryEdge* brun = bgate;

  while (count < offset)
  {
    brun = brun->boundary_next;
    count++;
  }
  return brun;
}

static BoundaryEdge* findOnStackBoundaryPrev(BoundaryEdge* bgate, int offset)
{
  int count = 0;
  BoundaryEdge* brun = bgate->boundary_prev;

  while (count < offset)
  {
    brun = brun->boundary_prev;
    count++;
  }
  return brun;
}

static void readPosition(int* n)
{
  codec->readPosition(n);
}

static void readPositionLastPredicted(int* l, int* n)
{
  codec->readPositionCorrectorLast(corr);
  pq->AddCorrector(l,corr,n);
}

static void readPositionAcrossPredicted(int* a, int* b, int* c, int* n)
{
  codec->readPositionCorrectorAcross(corr);
  VecAdd3iv(pred,a,c);
  VecSelfSubtract3iv(pred,b);
  pq->Clamp(pred);
  pq->AddCorrector(pred,corr,n);
}

static int decompressWithZeroSlots(PSreader_lowspan* ps, Boundary* boundary)
{
  int* across;

  BoundaryEdge* slide;

  BoundaryEdge* gate = boundary->gate;
  int boundary_length = boundary->length;
  int zero_slots = boundary->zero_slots;
  int one_slots = boundary->one_slots;

  gate = best_gate_zero(gate);
  slide = gate;
  gate = gate->boundary_next;

  if (slide->border == 0)
  {
    // output vertex 0
    output_vertex[0] = slide->vertex;
    // output edge 0->1
    ps->t_eflag[0] = PS_LAST; // leaving edge
    get_e_data[0] = slide->user_data;
    set_e_data[0] = (const void**)PS_UNDEFINED;
  }

  while (slide->slots == 0)
  {
    zero_slots--;
    boundary_length--;

    if (slide == gate)
    {
      boundary->gate = 0;
      boundary->zero_slots = 0;
      deallocBoundaryEdge(slide);
      ps->h_count++; // this must have been a hole
      return 0; // did not generate a triangle
    }
    else
    {
      slide->boundary_prev->boundary_next = gate;
      gate->boundary_prev = slide->boundary_prev;
      across = slide->vertex->origin;
      deallocBoundaryEdge(slide); // this does not really delete the memory; the slide->vertex->origin data is preserved
      slide = gate->boundary_prev;
      if (slide->border == 0)
      {
        // output vertex 0
        output_vertex[2] = slide->vertex;
        // output edge 2->0
        ps->t_eflag[2] = PS_LAST; // leaving edge
        get_e_data[2] = slide->user_data;
        set_e_data[2] = (const void**)PS_UNDEFINED;
      }
    }
  }

  // this was a hole -> then the zero slots must be used up here.
  if (slide->border)
  {
//      boundary->gate = slide->boundary_prev; // why did we do this? changed on 27/dec/03
    boundary->gate = gate;
    boundary->length = boundary_length;
    boundary->zero_slots = 0;
    boundary->one_slots = one_slots;
    return 0; // did not generate a triangle
  }

  // otherwise this was a face
  ps->f_count++;

  // two boundary degrees are used up:
  gate->slots--;
  slide->slots--;

  // ! UPDATE ! ZERO AND ONE SLOT COUNTS FOR "gate"
  if (gate->slots == 1)
  {
    if (gate->border == 0) // and not a border edge
    {
      one_slots++;
    }
  }
  else if (gate->slots == 0)
  {
    zero_slots++;
    if (gate->border == 0) // and not a border edge
    {
      one_slots--;
    }
  }

  VecCopy3iv(slide->across,across);
  if (gate->slots == 0)
  {
    slide->border = gate->border;
  }
  else if (slide->slots == 0)
  {
    slide->border = slide->boundary_prev->border;
  }
  else
  {
    slide->border = codec->readPolygonOrHole();
  }

  // ! UPDATE ! ZERO AND ONE SLOT COUNTS FOR "slide"
  if (slide->slots == 1)
  {
    if (slide->border == 0) // and not a border edge
    {
      one_slots++;
    }
  }
  else if (slide->slots == 0)
  {
    zero_slots++;
    one_slots--; // could not have been a border edge
  }

  // output vertex 1
  output_vertex[1] = gate->vertex;
  // output edge 1->2
  if (slide->border)
  {
    ps->t_eflag[1] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
    set_e_data[1] = (const void**)PS_UNDEFINED;
    get_e_data[1] = (const void*)PS_UNDEFINED;
  }
  else
  {
    ps->t_eflag[1] = PS_FIRST; // entering_edge
    set_e_data[1] = &(slide->user_data);
    get_e_data[1] = (const void*)PS_UNDEFINED;
  }

  boundary->gate = slide;
  boundary->length = boundary_length;
  boundary->zero_slots = zero_slots;
  boundary->one_slots = one_slots;
  return 1; // generated a triangle
}

static int decompress(PSreader_lowspan* ps)
{
  int has_free_vertex;
  int across[3];

  BoundaryVertex* vertex;

  BoundaryEdge* slide;
  BoundaryEdge* slideR;
  BoundaryEdge* slideL;

  while (true)
  {
    if (boundary)
    {
      has_free_vertex = decompressWithZeroSlots(ps, boundary);
      if (boundary->zero_slots == 0)
      {
        if (boundary->gate)
        {
          addBoundary(boundary);
        }
        else
        {
          deallocBoundary(boundary);
        }
        boundary = 0;
      }
      if (has_free_vertex)
      {
        return 1;
      }
    }

    if (gate == 0) // this means ...
    {
      has_left_zero_slot = false;
      if (hasMoreBoundaries()) // ... either we pop a boundary from the stack
      {
        Boundary* boundary = getBoundary();
        gate = boundary->gate;
        boundary_length = boundary->length;
        one_slots = boundary->one_slots;
        zero_slots = 0;
        deallocBoundary(boundary);

        // set the age of next boundary in queue
        queue_boundary_age = getQueueBoundaryAge();
      }
      else // ... or we start a new component
      {
        ps->c_count++;

        // set the age of next boundary in queue
        queue_boundary_age = getQueueBoundaryAge();

        // inititalize boundary
        gate = allocBoundaryEdge();
        gate->across[0] = -1;
        gate->border = codec->readPolygonOrHole();

        int v_degree = codec->readVertexDegree(); // read vertex degree
        gate->slots = v_degree - 1;
        vertex = gate->vertex = allocBoundaryVertex();

        if (codec->readPositionManifoldOrNot())
        {
          if (codec->readPositionFirstNonManifoldOccurance())
          {
            ps->t_vflag[0] = PS_FIRST | PS_RING_START; // first non-manifold vertex encounter
            vertex->index = ps->v_count; // assign vertex index
            vertex->use_count = USE_COUNT_INFLATE; // the use-count of the first occurance is inflated ... until there are no more non-manifold occurances
            readPosition(vertex->origin);  // read vertex position (center predicted)
            ps->v_count++;
            vertex->non_manifold = vertex; // marks vertex as non-manifold
            nonmanifoldDecompress->addElement((void*)vertex);
            ps->nm_v_count++;
          }
          else
          {
            int range = codec->readRange(nonmanifoldDecompress->size());
            BoundaryVertex* nm_vertex = (BoundaryVertex*) nonmanifoldDecompress->elementAt(range);
            // insert into linked list of non-manifolds
            vertex->non_manifold = nm_vertex->non_manifold;
            nm_vertex->non_manifold = vertex;
            // copy the data
            vertex->index = nm_vertex->index;
            vertex->user_data = nm_vertex->user_data;
            VecCopy3iv(vertex->origin,nm_vertex->origin);
            if (codec->readPositionMoreNonManifoldOccurances() == 0)
            {
              ps->t_vflag[0] = PS_RING_START; // last non-manifold vertex encounter
              nonmanifoldDecompress->lazyRemoveElementAt(range);
              // deflate USE_COUNT_INFLATE inflated use-count of nm_vertex
              nm_vertex->use_count -= USE_COUNT_INFLATE;
              // that might lead to nm_vertex being deallocated 
              if (nm_vertex->use_count == 0)
              {
                BoundaryVertex* runner = vertex;
                while (runner->non_manifold != nm_vertex) runner = runner->non_manifold;
                runner->non_manifold = vertex;
                deallocBoundaryVertex(nm_vertex);
              }
            }
            else
            {
              ps->t_vflag[0] = PS_RING_START; // | PS_RING_MORE; // another non-manifold vertex encounter
            }
          }
        }
        else
        {
          ps->t_vflag[0] = PS_FIRST; // new vertex
          vertex->index = ps->v_count; // assign vertex index
          readPosition(vertex->origin); // read vertex position (center predicted)
          ps->v_count++;
        }
        pq->DeQuantize(vertex->origin,vertex->origin_f);

        slide = allocBoundaryEdge();
        slide->across[0] = -1;
        slide->border = codec->readPolygonOrHole();
      
        v_degree = codec->readVertexDegree(); // read vertex degree
        slide->slots = v_degree - 1;
        vertex = slide->vertex = allocBoundaryVertex();

        if (codec->readPositionManifoldOrNot())
        {
          if (codec->readPositionFirstNonManifoldOccurance())
          {
            ps->t_vflag[1] = PS_FIRST | PS_RING_START; // first non-manifold vertex encounter
            vertex->index = ps->v_count; // assign vertex index
            vertex->use_count = USE_COUNT_INFLATE; // the use-count of the first occurance is inflated ... until there are no more non-manifold occurances
            readPositionLastPredicted(gate->vertex->origin,vertex->origin); // read vertex position (last predicted)
            ps->v_count++;
            vertex->non_manifold = vertex; // marks vertex as non-manifold
            nonmanifoldDecompress->addElement((void*)vertex);
            ps->nm_v_count++;
          }
          else
          {
            int range = codec->readRange(nonmanifoldDecompress->size());
            BoundaryVertex* nm_vertex = (BoundaryVertex*) nonmanifoldDecompress->elementAt(range);
            // insert into linked list of non-manifolds
            vertex->non_manifold = nm_vertex->non_manifold;
            nm_vertex->non_manifold = vertex;
            // copy the data
            vertex->index = nm_vertex->index;
            vertex->user_data = nm_vertex->user_data;
            VecCopy3iv(vertex->origin,nm_vertex->origin);
            if (codec->readPositionMoreNonManifoldOccurances() == 0)
            {
              ps->t_vflag[1] = PS_RING_START; // last non-manifold vertex encounter
              nonmanifoldDecompress->lazyRemoveElementAt(range);
              // deflate USE_COUNT_INFLATE inflated use-count of nm_vertex
              nm_vertex->use_count -= USE_COUNT_INFLATE;
              // that might lead to nm_vertex being deallocated 
              if (nm_vertex->use_count == 0)
              {
                BoundaryVertex* runner = vertex;
                while (runner->non_manifold != nm_vertex) runner = runner->non_manifold;
                runner->non_manifold = vertex;
                deallocBoundaryVertex(nm_vertex);
              }
            }
            else
            {
              ps->t_vflag[1] = PS_RING_START; // | PS_RING_MORE; // another non-manifold vertex encounter
            }
          }
        }
        else
        {
          ps->t_vflag[1] = PS_FIRST; // new vertex
          vertex->index = ps->v_count; // assign vertex index
          readPositionLastPredicted(gate->vertex->origin,vertex->origin); // read vertex position (last predicted)
          ps->v_count++;
        }
        pq->DeQuantize(vertex->origin,vertex->origin_f);

        gate->boundary_next = slide;
        gate->boundary_prev = slide;

        slide->boundary_next = gate;
        slide->boundary_prev = gate;

        boundary_length = 2;
        zero_slots = 0;
        one_slots = ((gate->slots == 1) && (gate->border == 0)) + ((slide->slots == 1) && (slide->border == 0));
      }
    }
  
    if (ps->v_count >= 1065119)
    {
      ps->v_count = ps->v_count;
    }

    gate = best_gate(gate, zero_slots, has_left_zero_slot);

    slide = gate;
    gate = gate->boundary_next;

    if (slide->border == 0)
    {
      // output vertex 0
      output_vertex[0] = slide->vertex;
      // output edge 0->1
      ps->t_eflag[0] = PS_LAST; // leaving_edge
      get_e_data[0] = slide->user_data;
      set_e_data[0] = (const void**)PS_UNDEFINED;
    }

    // can we trivially include this face ?
    has_free_vertex = 1;

    while (slide->slots == 0)
    {
      zero_slots--;
      boundary_length--;

      if (slide == gate) // boundary ends around ...
      {
        gate = 0;
        deallocBoundaryEdge(slide); // CAREFUL: this only overwrites the slide->boundary_next pointer
                                    //          which now addresses the previously dealloced boundary
                                    //          edge. we could actually use this as a trick to address 
                                    //          the data of already dealloced edges

        if (slide->border == 0) // ... a face
        {
          ps->f_count++;

          // output vertex 1
          output_vertex[1] = slide->vertex;
          // output edge 1->2
          ps->t_eflag[1] = PS_LAST; // leaving_edge
          get_e_data[1] = slide->user_data;
          set_e_data[1] = (const void**)PS_UNDEFINED;
          return 1; // we already produced a triangle
        }
        else                    // ... a hole
        {
          ps->h_count++;
          break; // we did not yet produce a triangle
        }
      }
      else
      {
        slide->boundary_prev->boundary_next = gate;
        gate->boundary_prev = slide->boundary_prev;
        VecCopy3iv(across,slide->vertex->origin);
        deallocBoundaryEdge(slide);
        slide = gate->boundary_prev;
        if (has_free_vertex)
        {
          has_free_vertex = 0;
          if (slide->border == 0)
          {
            // output vertex 2
            output_vertex[2] = slide->vertex;
            // output edge 2->0
            ps->t_eflag[2] = PS_LAST; // leaving_edge
            get_e_data[2] = slide->user_data;
            set_e_data[2] = (const void**)PS_UNDEFINED;
          }
        }
      }
    }

    // this was a hole -> search on
    if (slide->border)
    {
      has_left_zero_slot = false;
//    if (gate)   // why had we done this ...???
//    {
//      gate = slide->boundary_prev;
//    }
      continue;
    }

    ps->f_count++;

    // two boundary degrees are used up:
    gate->slots--;
    slide->slots--;

    // ! UPDATE ! ZERO AND ONE SLOT COUNTS FOR "gate"
    if (gate->slots == 1)
    {
      if (gate->border == 0) // and not a border edge
      {
        one_slots++;
      }
    }
    else if (gate->slots == 0)
    {
      zero_slots++;
      if (gate->border == 0) // and not a border edge
      {
        one_slots--;
      }
    }

    if ( has_free_vertex )
    {
      VecCopy3iv(across,slide->across);

      slideL = slide;
      VecCopy3iv(slideL->across,gate->vertex->origin);

      slideR = allocBoundaryEdge();
      VecCopy3iv(slideR->across,slide->vertex->origin);

      slideR->boundary_next = gate;
      gate->boundary_prev = slideR;

      if (codec->readAddOrSplitMerge() == 1) // split or merge
      {
            if (codec->s == 2042)
            {
              codec->s = 2042;
            }

        BoundaryEdge* glue;
        if (codec->readSplitOrMerge() == 0) // split
        {
          int direction = codec->readSplitDirection(2);
          int offset = codec->readSplitOffset(boundary_length/2);
          int split_one_slots;
          if (direction)
          {
            glue = findOnThisBoundaryNext(gate, offset, split_one_slots);
          }
          else
          {
            glue = findOnThisBoundaryPrev(slideL, offset, split_one_slots);
          }
          int remaining_slots = codec->readSplitRemaining(glue->slots-1);

          slideR->slots = glue->slots - remaining_slots - 2;
          glue->slots = remaining_slots;

          // IS "slideL" A BORDER EDGE OR NOT?
          if (slideL->slots == 0)
          {
            slideL->border = slideL->boundary_prev->border;
          }
          else if (glue->slots == 0)
          {
            slideL->border = glue->border;
          }
          else
          {
            slideL->border = codec->readPolygonOrHole();
          }

          // IS "slideR" A BORDER EDGE OR NOT?
          if (slideR->slots == 0)
          {
            slideR->border = glue->boundary_prev->border;
          }
          else if (gate->slots == 0)
          {
            slideR->border = gate->border;
          }
          else
          {
            slideR->border = codec->readPolygonOrHole();
          }

          // this actually splits it into two loops

          slideR->vertex = glue->vertex;

          slideR->boundary_prev = glue->boundary_prev;
          glue->boundary_prev->boundary_next = slideR;

          glue->boundary_prev = slideL;
          slideL->boundary_next = glue;

          // output vertex 1
          output_vertex[1] = gate->vertex;

          // output edge 1->2
          if (slideR->border)
          {
            ps->t_eflag[1] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
            set_e_data[1] = (const void**)PS_UNDEFINED;
            get_e_data[1] = (const void*)PS_UNDEFINED;
          }
          else
          {
            ps->t_eflag[1] = PS_FIRST; // entering_edge
            set_e_data[1] = &(slideR->user_data);
            get_e_data[1] = (const void*)PS_UNDEFINED;
          }

          if (direction)
          {
            boundary = allocBoundary(slideL, boundary_length - offset - 1, (slideL->slots == 0)+(glue->slots == 0), one_slots-split_one_slots-(slideL->slots == 0)+((glue->slots == 1)&&(glue->border == 0))+((slideL->slots == 1)&&(slideL->border == 0)), glue->vertex->index);
            one_slots = split_one_slots+((slideR->slots == 1)&&(slideR->border == 0));
            boundary_length = offset + 2;
          }
          else
          {
            boundary = allocBoundary(slideL, offset + 2, (slideL->slots == 0)+(glue->slots == 0), split_one_slots+((glue->slots == 1)&&(glue->border == 0))+((slideL->slots == 1)&&(slideL->border == 0)), glue->vertex->index);
            one_slots = one_slots-split_one_slots-(slideL->slots == 0)+((slideR->slots == 1)&&(slideR->border == 0));
            boundary_length = boundary_length - offset - 1;
          }
          if (!boundary->zero_slots)
          {
            addBoundary(boundary);
            boundary = 0;
          }
          zero_slots = (slideR->slots == 0)+(gate->slots == 0);
          gate = slideR;
        }
        else // merge
        {
          int index = codec->readMergeIndex(numberBoundaries());
          boundary = removeBoundary(index);
          int direction = codec->readMergeDirection(2);
          int offset = codec->readMergeOffset((boundary->length+1)/2);
          if (direction)
          {
            glue = findOnStackBoundaryNext(boundary->gate, offset);
          }
          else
          {
            glue = findOnStackBoundaryPrev(boundary->gate, offset);
          }
          int remaining_slots = codec->readMergeRemaining(glue->slots - 1);
          
          slideR->slots = glue->slots - remaining_slots - 2;
          glue->slots = remaining_slots;

          // IS "slideL" A BORDER EDGE OR NOT?
          if (slideL->slots == 0)
          {
            slideL->border = slideL->boundary_prev->border;
          }
          else if (glue->slots == 0)
          {
            slideL->border = glue->border;
          }
          else
          {
            slideL->border = codec->readPolygonOrHole();
          }

          // ! UPDATE ! ZERO AND ONE SLOT COUNTS FOR "slideL"
          if (slideL->slots == 1)
          {
            if (slideL->border == 0) // and not a border edge
            {
              one_slots++;
            }
          }
          else if (slideL->slots == 0)
          {
            zero_slots++;
            one_slots--; // could not have been a border edge
          }

          // IS "slideR" A BORDER EDGE OR NOT?
          if (slideR->slots == 0)
          {
            slideR->border = glue->boundary_prev->border;
          }
          else if (gate->slots == 0)
          {
            slideR->border = gate->border;
          }
          else
          {
            slideR->border = codec->readPolygonOrHole();
          }

          // this actually merges it into one loop

          slideR->vertex = glue->vertex;

          slideR->boundary_prev = glue->boundary_prev;
          glue->boundary_prev->boundary_next = slideR;

          glue->boundary_prev = slideL;
          slideL->boundary_next = glue;

          // output vertex 1
          output_vertex[1] = gate->vertex;
          // output edge 1->2
          if (slideR->border)
          {
            ps->t_eflag[1] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
            set_e_data[1] = (const void**)PS_UNDEFINED;
            get_e_data[1] = (const void*)PS_UNDEFINED;
          }
          else
          {
            ps->t_eflag[1] = PS_FIRST; // entering_edge
            set_e_data[1] = &(slideR->user_data);
            get_e_data[1] = (const void*)PS_UNDEFINED;
          }

          zero_slots += (glue->slots == 0) + (slideR->slots == 0);
          one_slots += boundary->one_slots + ((glue->slots == 1) && (glue->border == 0)) + ((slideR->slots == 1) && (slideR->border == 0));

          boundary_length = boundary_length + boundary->length + 1;
          deallocBoundary(boundary);
          boundary = 0;
          gate = slideR;
        }

        // output vertex 2
        output_vertex[2] = slideR->vertex;
        // output edge 1->2
        if (slideL->border)
        {
          ps->t_eflag[2] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
          set_e_data[2] = (const void**)PS_UNDEFINED;
          get_e_data[2] = (const void*)PS_UNDEFINED;
        }
        else
        {
          ps->t_eflag[2] = PS_FIRST; // entering_edge
          set_e_data[2] = &(slideL->user_data);
          get_e_data[2] = (const void*)PS_UNDEFINED;
        }
      }
      else // add
      {
        boundary_length++;

        // GET DEGREE OF ORIGIN VERTEX OF "slideR"
        int v_degree = codec->readVertexDegree(); // read vertex degree
        slideR->slots = v_degree - 2;

        // IS "slideL" A BORDER EDGE OR NOT?
        if (slideL->slots == 0)
        {
          slideL->border = slideL->boundary_prev->border;
        }
        else
        {
          slideL->border = codec->readPolygonOrHole(); 
        }

        // ! UPDATE ! ZERO AND ONE SLOT COUNTS FOR "slideL"
        if (slideL->slots == 1)
        {
          if (slideL->border == 0) // and not a border edge
          {
            one_slots++;
          }
        }
        else if (slideL->slots == 0)
        {
          zero_slots++;
          has_left_zero_slot = true;    // added 27/dec/03
          one_slots--; // could not have been a border edge
        }

        // IS "slideR" A BORDER EDGE OR NOT?
        if (gate->slots == 0)
        {
          slideR->border = gate->border;
        }
        else if (slideR->slots == 0)
        {
          slideR->border = slideL->border;
        }
        else
        {
          slideR->border = codec->readPolygonOrHole(); 
        }

        // ! ADD ! ZERO AND ONE SLOT COUNTS FOR "slideR"
        if (slideR->slots == 1)
        {
          if (slideR->border == 0) // and not a border edge
          {
            one_slots++;
          }
        }
        else if (slideR->slots == 0)
        {
          zero_slots++;
        }

        // output vertex 1
        output_vertex[1] = gate->vertex;
        // output edge 1->2
        if (slideR->border)
        {
          ps->t_eflag[1] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
          set_e_data[1] = (const void**)PS_UNDEFINED;
          get_e_data[1] = (const void*)PS_UNDEFINED;
        }
        else
        {
          ps->t_eflag[1] = PS_FIRST; // entering_edge
          set_e_data[1] = &(slideR->user_data);
          get_e_data[1] = (const void*)PS_UNDEFINED;
        }

        slideL->boundary_next = slideR;
        slideR->boundary_prev = slideL;

        vertex = slideR->vertex = allocBoundaryVertex();

        if (codec->readPositionManifoldOrNot())
        {
          if (codec->readPositionFirstNonManifoldOccurance())
          {
            ps->t_vflag[2] = PS_FIRST | PS_RING_START; // first non-manifold vertex encounter
            vertex->index = ps->v_count; // assign vertex index
            vertex->use_count = USE_COUNT_INFLATE; // the use-count of the first occurance is inflated ... until there are no more non-manifold occurances

            // for the start operation we have to do something special. the -1 determines a start configuration.
            if (gate->across[0] == -1)
            {
              readPositionLastPredicted(slideL->vertex->origin,vertex->origin); // read position (last predicted)
              ps->v_count++;
              VecCopy3iv(gate->across,vertex->origin);

              // then we also need to fix this, which was set to the wrong value earlier ...
              get_e_data[0] = (const void*)PS_UNDEFINED;
              if (gate->border)
              {
                ps->t_eflag[0] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
              }
              else
              {
                ps->t_eflag[0] = PS_FIRST; // entering_edge
                set_e_data[0] = &(gate->user_data);
              }
            }
            else
            {
              readPositionAcrossPredicted(slideL->vertex->origin,across,gate->vertex->origin,vertex->origin); // read position (across predicted)
              ps->v_count++;
            }
            vertex->non_manifold = vertex;
            nonmanifoldDecompress->addElement(vertex);
            ps->nm_v_count++;
          }
          else
          {
            int range = codec->readRange(nonmanifoldDecompress->size());
            BoundaryVertex* nm_vertex = (BoundaryVertex*) nonmanifoldDecompress->elementAt(range);
            // insert into linked list of non-manifolds
            vertex->non_manifold = nm_vertex->non_manifold;
            nm_vertex->non_manifold = vertex;
            // copy the data
            vertex->index = nm_vertex->index;
            vertex->user_data = nm_vertex->user_data;
            VecCopy3iv(vertex->origin,nm_vertex->origin);
            if (codec->readPositionMoreNonManifoldOccurances() == 0)
            {
              ps->t_vflag[2] = PS_RING_START; // last non-manifold vertex encounter
              nonmanifoldDecompress->lazyRemoveElementAt(range);
              // deflate USE_COUNT_INFLATE inflated use-count of nm_vertex
              nm_vertex->use_count -= USE_COUNT_INFLATE;
              // that might lead to nm_vertex being deallocated 
              if (nm_vertex->use_count == 0)
              {
                BoundaryVertex* runner = vertex;
                while (runner->non_manifold != nm_vertex) runner = runner->non_manifold;
                runner->non_manifold = vertex;
                deallocBoundaryVertex(nm_vertex);
              }
            }
            else
            {
              ps->t_vflag[2] = PS_RING_START; // | PS_RING_MORE; // another non-manifold vertex encounter
            }

            // for the start operation we have to do some fixing. the -1 determines a start configuration.
            if (gate->across[0] == -1)
            {
              VecCopy3iv(gate->across,vertex->origin);

              // we also need to fix this, which was set to the wrong value earlier ...
              get_e_data[0] = (const void*)PS_UNDEFINED;
              if (gate->border)
              {
                ps->t_eflag[0] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
              }
              else
              {
                ps->t_eflag[0] = PS_FIRST; // entering_edge
                set_e_data[0] = &(gate->user_data);
              }
            }
          }
        }
        else
        {
          ps->t_vflag[2] = PS_FIRST; // new vertex
          vertex->index = ps->v_count; // assign vertex index
          
          // for the start operation we have to do something special. the -1 determines a start configuration.
          if (gate->across[0] == -1)
          {
            readPositionLastPredicted(slideL->vertex->origin,vertex->origin); // read position (last predicted)
            ps->v_count++;
            VecCopy3iv(gate->across,vertex->origin);

            // we also need to fix this, which was set to the wrong value earlier ...
            get_e_data[0] = (const void*)PS_UNDEFINED;
            if (gate->border)
            {
              ps->t_eflag[0] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
            }
            else
            {
              ps->t_eflag[0] = PS_FIRST; // entering_edge
              set_e_data[0] = &(gate->user_data);
            }
          }
          else
          {
            readPositionAcrossPredicted(slideL->vertex->origin,across,gate->vertex->origin,vertex->origin); // read position (across predicted)
            ps->v_count++;
          }
        }
        pq->DeQuantize(vertex->origin,vertex->origin_f);

        gate = slideR;

        // output vertex 2
        output_vertex[2] = slideR->vertex;
        // output edge 2->0
        if (slideL->border)
        {
          ps->t_eflag[2] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
          set_e_data[2] = (const void**)PS_UNDEFINED;
          get_e_data[2] = (const void*)PS_UNDEFINED;
        }
        else
        {
          ps->t_eflag[2] = PS_FIRST; // entering_edge
          set_e_data[2] = &(slideL->user_data);
          get_e_data[2] = (const void*)PS_UNDEFINED;
        }
      }
    }
    else // fill
    {
      VecCopy3iv(slide->across, across);

      // is that a left zero slot we were dealing with?
      if (has_left_zero_slot)
      {
        has_left_zero_slot = false;
        was_right_zero_slot = false;
      }
      else
      {
        was_right_zero_slot = true;
      }

      if (gate->slots == 0)
      {
        slide->border = gate->border;
      }
      else if (slide->slots == 0)
      {
        slide->border = slide->boundary_prev->border;
      }
      else
      {
        slide->border = codec->readPolygonOrHole();
      }
      // ! UPDATE ! ZERO AND ONE SLOT COUNTS FOR "slide"
      if (slide->slots == 1)
      {
        if (slide->border == 0) // and not a border edge
        {
          one_slots++;
        }
      }
      else if (slide->slots == 0)
      {
        zero_slots++;
        has_left_zero_slot = true;    // added 27/dec/03
        one_slots--; // could not have been a border edge
      }

      // output vertex 1
      output_vertex[1] = gate->vertex;
      // output edge 1->2
      if (slide->border)
      {
        ps->t_eflag[1] = PS_FIRST|PS_LAST|PS_BORDER; // border_edge
        set_e_data[1] = (const void**)PS_UNDEFINED;
        get_e_data[1] = (const void*)PS_UNDEFINED;
      }
      else
      {
        ps->t_eflag[1] = PS_FIRST; // entering_edge
        set_e_data[1] = &(slide->user_data);
        get_e_data[1] = (const void*)PS_UNDEFINED;
      }

      // here we check if we
      //   (1) resolved a left zero and need to move the gate
      //   (2) have to switch to a 'younger' boundary from the queue

      if (zero_slots)
      {
        gate = slide;
      }
      else if (was_right_zero_slot)
      {
        if (gate->vertex->index > queue_boundary_age)
        {
          Boundary* boundary = allocBoundary(slide, boundary_length, 0, one_slots, gate->vertex->index);
          addBoundary(boundary);
          gate = 0;
        }
        else
        {
          gate = slide;
        }
      }
    }
    return 1; // we already produced a triangle
  }
}

bool PSreader_lowspan::open(const char* file_name)
{
  FILE* fp = fopen(file_name, "rb");
  if (fp == 0)
  {
    fprintf(stderr, "ERROR: cannot open file '%s'\n",file_name);
    return false;
  }
  return open(fp);
}

bool PSreader_lowspan::open(FILE* fp)
{
  int i;
  int elem_count;
  char *elem_name;

  PlyFile* in_ply = read_ply(fp);

  if (in_ply == 0)
  {
    fprintf(stderr, "ERROR: corrupt input file\n");
    return false;
  }

  nverts = -1;
  nfaces = -1;
  ncomps = -1;
  bits = -1;

  // the nverts, nfaces, and bits are in a comment
  for (i = 0; i < in_ply->num_comments; i++)
  {
    if (strncmp(in_ply->comments[i],"nverts",6) == 0)
    {
      sscanf(in_ply->comments[i], "nverts %d", &nverts);
      PRINT_CONTROL_OUTPUT(stderr,"nverts %d\n",nverts);
    }
    else if (strncmp(in_ply->comments[i],"nfaces",6) == 0)
    {
      sscanf(in_ply->comments[i], "nfaces %d", &nfaces);
      PRINT_CONTROL_OUTPUT(stderr,"nfaces %d\n",nfaces);
    }
    else if (strncmp(in_ply->comments[i],"ncomps",6) == 0)
    {
      sscanf(in_ply->comments[i], "ncomps %d", &ncomps);
      PRINT_CONTROL_OUTPUT(stderr,"ncomps %d\n",ncomps);
    }
    else if (strncmp(in_ply->comments[i],"bits",4) == 0)
    {
      sscanf(in_ply->comments[i], "bits %d", &bits);
      PRINT_CONTROL_OUTPUT(stderr,"bits %d\n",bits);
    }
  }

  if ((nverts == -1) || (nfaces == -1) || (bits == -1))
  {
    fprintf(stderr, "ERROR: something is missing in the comments\n");
    fprintf(stderr, "nverts == %d  nfaces == %d  bits == %d\n",nverts, nfaces, bits);
    return false;
  }

  for (i = 0; i < in_ply->num_elem_types; i++)
  {
    /* prepare to read the i'th list of elements */
    elem_name = setup_element_read_ply(in_ply, i, &elem_count);

    if (equal_strings ("boundingbox", elem_name))
    {
      PRINT_CONTROL_OUTPUT(stderr,"reading %d bounding box coordinates\n",elem_count);

      /* set up for getting boundingbox elements */

      setup_property_ply(in_ply, &boundingbox_props[0]);
      
      pq = new PositionQuantizer();

      /* grab all the boundingbox elements */
      get_element_ply (in_ply, (void *)&(pq->m_afMin[0]));
      get_element_ply (in_ply, (void *)&(pq->m_afMin[1]));
      get_element_ply (in_ply, (void *)&(pq->m_afMin[2]));
      get_element_ply (in_ply, (void *)&(pq->m_afMax[0]));
      get_element_ply (in_ply, (void *)&(pq->m_afMax[1]));
      get_element_ply (in_ply, (void *)&(pq->m_afMax[2]));

      PRINT_CONTROL_OUTPUT(stderr,"bb min %15.5f %15.5f %15.5f\n",pq->m_afMin[0], pq->m_afMin[1], pq->m_afMin[2]);
      PRINT_CONTROL_OUTPUT(stderr,"bb max %15.5f %15.5f %15.5f\n",pq->m_afMax[0], pq->m_afMax[1], pq->m_afMax[2]);

      pq->SetPrecision(bits);
      pq->SetupQuantizer();

      bb_min_f = pq->m_afMin;
      bb_max_f = pq->m_afMax;

      bb_min_i = pq->m_aiMinCode;
      bb_max_i = pq->m_aiMaxCode;
    }
    else if (equal_strings ("code", elem_name))
    {
      if (pq == 0)
      {
        fprintf(stderr, "ERROR: position quantizer not initialized\n");
        return false;
      }
      else
      {
        break;
      }
    }
  }

  boundaryVertices = 0;

  numBoundaryVerticesAlloced = 0;
  maxBoundaryVerticesAlloced = 0;

  boundaryEdges = 0;

  numBoundaryEdgesAlloced = 0;
  maxBoundaryEdgesAlloced = 0;
  
  boundaries = 0;

  numBoundariesAlloced = 0;
  maxBoundariesAlloced = 0;

  // init counters / state

  v_count = 0;
  f_count = 0;
  h_count = 0;
  c_count = 0;

  nm_v_count = 0;
  nm_e_count = -1; // not supported

  gate = 0;

  boundary = 0;

  boundaryQueue = new CrazyVector();
  codec = new TSCcodec();
  nonmanifoldDecompress = new CrazyVector();

  PRINT_CONTROL_OUTPUT(stderr,"starting decompression ...\n");
  codec->initDec(10,pq->m_uBits,pq->m_aiRangeCode,pq->m_aiAbsRangeCorrector,in_ply->fp);

  free_ply (in_ply);
  return true;
}

PSevent PSreader_lowspan::read_triangle()
{
  if (f_count == nfaces)
  {
    return PS_EOF;
  }

  t_orig = f_count;

  t_vflag[0] = 0;
  t_vflag[1] = 0;
  t_vflag[2] = 0;

  decompress(this);

  for (int i = 0; i < 3; i++)
  {
    t_idx[i] = output_vertex[i]->index;
    t_pos_i[i] = output_vertex[i]->origin;
    t_pos_f[i] = output_vertex[i]->origin_f;

    if (t_eflag[i] == PS_FIRST)
    {
      output_vertex[i]->use_count += 1;
    }
    else if (t_eflag[i] == PS_LAST)
    {
      output_vertex[i]->use_count -= 1;
    }

    if (t_eflag[(i+2)%3] == PS_FIRST)
    {
      output_vertex[i]->use_count += 1;
    }
    else if (t_eflag[(i+2)%3] == PS_LAST)
    {
      output_vertex[i]->use_count -= 1;
    }

    if (t_vflag[i] & PS_FIRST)
    {
      set_v_data[i] = &(output_vertex[i]->user_data);
      get_v_data[i] = (const void*)PS_UNDEFINED;
    }
    else
    {
      set_v_data[i] = (const void**)PS_UNDEFINED;
      get_v_data[i] = output_vertex[i]->user_data;
    }

    if (output_vertex[i]->non_manifold)
    {
      if (t_vflag[i] & PS_RING_START)
      {
        set_v_data[i] = &(output_vertex[i]->user_data);
      }

      if (output_vertex[i]->use_count == USE_COUNT_INFLATE)
      {
        t_vflag[i] |= (PS_NON_MANIFOLD|PS_RING_END);
      }
      else
      {
        t_vflag[i] |= (PS_NON_MANIFOLD);
      }
    }

    if (output_vertex[i]->use_count == 0)
    {
      if (output_vertex[i]->non_manifold)
      {
        if (output_vertex[i]->non_manifold == output_vertex[i])
        {
          t_vflag[i] |= (PS_LAST|PS_RING_END);
        }
        else
        {
          BoundaryVertex* runner = output_vertex[i]->non_manifold;
          while (runner->non_manifold != output_vertex[i]) runner = runner->non_manifold;
          runner->non_manifold = output_vertex[i]->non_manifold;
          t_vflag[i] |= PS_RING_END;
        }
      }
      else
      {
        t_vflag[i] |= PS_LAST;
      }
      deallocBoundaryVertex(output_vertex[i]);
    }
  }
  return PS_TRIANGLE;
}

void PSreader_lowspan::set_vdata(const void* data, int i)
{
  (*(set_v_data[i])) = data;
}

void* PSreader_lowspan::get_vdata(int i) const
{
  return (void*)get_v_data[i];
}

void PSreader_lowspan::set_edata(const void* data, int i)
{
  (*(set_e_data[i])) = data;
}

void* PSreader_lowspan::get_edata(int i) const
{
  return (void*)get_e_data[i];
}

void PSreader_lowspan::close()
{
  PRINT_CONTROL_OUTPUT(stderr,"v: %d s: %d m: %d\n",codec->v,codec->s,codec->m);
  PRINT_CONTROL_OUTPUT(stderr,"v_count: %d f_count: %d h_count: %d c_count: %d nm_v_count: %d\n",v_count,f_count,h_count,c_count,nm_v_count);
  PRINT_CONTROL_OUTPUT(stderr,"\n\n");

  PRINT_CONTROL_OUTPUT(stderr,"maxBoundaryVerticesAlloced %d %d\n",maxBoundaryVerticesAlloced, numBoundaryVerticesAlloced);
  PRINT_CONTROL_OUTPUT(stderr,"maxBoundaryEdgesAlloced %d %d\n",maxBoundaryEdgesAlloced, numBoundaryEdgesAlloced);
  PRINT_CONTROL_OUTPUT(stderr,"maxBoundariesAlloced %d %d\n",maxBoundariesAlloced, numBoundariesAlloced);

  codec->doneDec();

  delete codec;
  delete boundaryQueue;
  delete pq;
  delete nonmanifoldDecompress; 

  if (v_count != nverts)
  {
    fprintf(stderr, "ERROR: wrong vertex count: v_count (%d) != nverts (%d)\n", v_count, nverts);
  }
  if (f_count != nfaces)
  {
    fprintf(stderr, "ERROR: wrong face count: f_count (%d) != nfaces (%d)\n", f_count, nfaces);
  }
  if ((ncomps != -1) && (c_count != ncomps))
  {
    fprintf(stderr, "ERROR: wrong component count: c_count (%d) != ncomps (%d)\n", c_count, ncomps);
  }
}

PSreader_lowspan::PSreader_lowspan()
{
  int i;

  v_idx = PS_UNDEFINED;
  v_vflag = PS_UNDEFINED;
  t_orig = PS_UNDEFINED;

  t_idx_orig = &(t_idx[0]);

  for (i = 0; i < 3; i++)
  {
    t_idx[i] = PS_UNDEFINED;
    t_pos_f[i] = 0;
    t_pos_i[i] = 0;
    t_vflag[i] = PS_UNDEFINED;
    t_eflag[i] = PS_UNDEFINED;

    set_v_data[i] = (const void**)PS_UNDEFINED;
    get_v_data[i] = (const void*)PS_UNDEFINED;

    set_e_data[i] = (const void**)PS_UNDEFINED;
    get_e_data[i] = (const void*)PS_UNDEFINED;
  }

  bb_min_f = 0;
  bb_max_f = 0;

  bb_min_i = 0;
  bb_max_i = 0;
}

PSreader_lowspan::~PSreader_lowspan()
{
}
