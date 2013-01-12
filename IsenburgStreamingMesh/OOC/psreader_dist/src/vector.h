/*
===============================================================================

  FILE:  tscvector.h
  
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
  
    28 June 2000 -- Martin - finalized for GI submission.
  
===============================================================================
*/
#ifndef TSC_VECTOR_H
#define TSC_VECTOR_H

class TSCvector
{
public:
  TSCvector();
  ~TSCvector();

  int isEmpty();
  int isElement(void *);
  
  void ensureCapacity(int);
  
  int size() const;
  void setSize(int s);
  
  void* elementAt(int) const;
  void* lastElement();
  
  void addElement(void*);
  void setElementAt(void*, int);
  void insertElementAt(void*, int);
  
  void removeElement(void*);
  void removeElementAt(int);
  void removeLastElement();
  void removeAllElements();

  // should be private
  
  void** data;
  int current_capacity;
  int current_size;
  
  void checkCapacity();
};

#endif
