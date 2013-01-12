/*
===============================================================================

  FILE:  SMreader_ply.h
  
  CONTENTS:
  
    Reads a vertices and triangles of a mesh in Stanford's non-streaming PLY
    format and provides access to it in form of a Streaming Mesh of maximal
    width.
    The open function takes two additional flags: One allows to skip all the
    vertices. This is a more efficient way then reading all the vertices when
    we only want to get SMtriangle events.
    The other allows to first compute the bounding box. Because this is
    implemented with fseek() the input stream must allow backwards seeking.
    Therefore gipped input streams cannot make use of this option directly.
    To compute a bounding box for those you need to open the gzipped file
    twice. The first time you only read the bounding box with skipping the
    vertices enabled. Then you close the reader and re-open it again, this
    time with both flags being false. The bounding box info is still there.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2003  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    17 January 2004 -- v_idx was not being set correctlty 
    6 January 2004 -- added support to compute the bounding box
    22 December 2003 -- initial version created once christmas plans were off
  
===============================================================================
*/
#ifndef SMREADER_PLY_H
#define SMREADER_PLY_H

#include "smreader.h"

#include <stdio.h>

class SMreader_ply : public SMreader
{
public:

  // smreader interface function implementations

  void close();

  SMevent read_element();
  SMevent read_event();

  // smreader_ply functions

  bool open(FILE* fp, bool compute_bounding_box=false, bool skip_vertices=false);

  SMreader_ply();
  ~SMreader_ply();

private:
  int velem;
  int felem;
};

#endif
