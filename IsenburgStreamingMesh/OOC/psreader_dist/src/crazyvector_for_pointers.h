/*
===============================================================================

  FILE:  crazyvector_for_pointers.h
  
  CONTENTS:
  
    Triangle Strip Compression - GI 2000 demonstration software
  
  PROGRAMMERS:
  
    Martin Isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    Copyright (C) 2000  Martin Isenburg (isenburg@cs.unc.edu)
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    19 January 2003 -- Martin - finalized for SIGGRAPH 03 submission.
                              - soon to go completely insane
  
===============================================================================
*/
#ifndef CRAZY_VECTOR_FOR_POINTERS_H
#define CRAZY_VECTOR_FOR_POINTERS_H

class CrazyVector
{
public:
  CrazyVector();
  ~CrazyVector();

  int size() const;
  void* elementAt(int i) const;
  void* firstElement() const;
  void addElement(const void* d);
  void lazyRemoveElementAt(int i);
  void removeFirstElement();

  // should be private
  
  const void** data;
  int current_capacity;
  int current_capacity_mask;
  int current_begin;
  int current_end;
  int current_size;
};

#endif
