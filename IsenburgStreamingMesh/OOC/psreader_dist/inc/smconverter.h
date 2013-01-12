/*
===============================================================================

  FILE:  SMconverter.h
  
  CONTENTS:
  
    Reads a mesh from a Processing Sequence and provides access to it in form
    of a Streaming Mesh. While this surely is a reduction of functionality it
    may still be a useful tool to have. For example, one may want to access an
    out-of-core compressed (SIGGRAPH 2003) mesh as a Streaming Mesh.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    30 October 2003 -- switched to enums and bools in peter's office
    02 October 2003 -- added functionality for read_element() 
    15 September 2003 -- initial version created on the Monday after a tough
                         hiking weekend in Yosemite
  
===============================================================================
*/
#ifndef SMCONVERTER_H
#define SMCONVERTER_H

#include "smreader.h"
#include "psreader.h"

class SMconverter : public SMreader
{
public:

  // smreader interface function implementations

  void close();

  SMevent read_element();
  SMevent read_event();

  // smreader_converter functions

  bool open(PSreader* psreader);

  SMconverter();
  ~SMconverter();

private:
  PSreader* psreader;
  int have_new, next_new;
  int new_vertices[3];
  int have_triangle;
  int have_finalized, next_finalized;
  int finalized_vertices[3];
};

#endif
