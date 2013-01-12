/*
===============================================================================

  FILE:  tscvector.cpp
  
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
#include "vector.h"

#include <stdlib.h>
#include <iostream.h>

TSCvector::TSCvector()
{
  current_size = 0;
  current_capacity = 1000;
  data = (void**) malloc(sizeof(void*)*1000);
}

TSCvector::~TSCvector()
{
  free(data);
}

int TSCvector::isEmpty()
{
  if (current_size == 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int TSCvector::isElement(void* e)
{
  for (int i = 0; i < current_size; i++)
  {
    if (data[i] == e)
    {
      return 1;
    }
  }
  return 0;
}

void TSCvector::ensureCapacity(int c)
{
  if (current_capacity < c)
  {
    void** temp = (void**) malloc(sizeof(void*)*c);
    for (int i = 0; i < current_size; i++)
    {
      temp[i] = data[i];
    }
    current_capacity = c;
    free(data);
    data = temp;
  }
}

int TSCvector::size() const
{
  return current_size;
}

void TSCvector::setSize(int s)
{
  ensureCapacity(s);
  current_size = s;
}

void* TSCvector::elementAt(int p) const
{
  if ((p < 0) || (p >= current_size))
  {
    return 0;
  }
  else
  {
    return data[p];
  }
}

void* TSCvector::lastElement()
{
  if (current_size == 0)
  {
    return 0;
  }
  else
  {
    return data[current_size-1];
  }
}

void TSCvector::addElement(void* e)
{
  checkCapacity();
  data[current_size] = e;
  current_size++;
}

void TSCvector::setElementAt(void* e, int p)
{
  if ((p < 0) || (p >= current_size))
  {
    cerr << "ERROR in TSCvector::setElementAt" << endl;
  }
  else
  {
    data[p] = e;
  }
}

void TSCvector::insertElementAt(void* e, int p)
{
  if ((p < 0) || (p > current_size))
  {
    cerr << "ERROR in TSCvector::insertElementAt" << endl;
  }
  else
  {
    checkCapacity();
    for (int i = current_size; i > p; i--)
    {
      data[i] = data[i-1];
    }
    data[p] = e;
    current_size++;
  }
}

void TSCvector::removeElement(void* e)
{
  int i, p = -1;
  for (i = current_size-1; i >= 0; i--)
  {
    if (e == data[i])
    {
      p = i;
      break;
    }
  }
  
  if (p == -1)
  {
    return;
  }
  
  for (i = p; i < current_size-1; i++)
  {
    data[i] = data[i+1];
  }
  current_size--;
}

void TSCvector::removeElementAt(int p)
{
  if ((p < 0) || (p >= current_size))
  {
    return;
  }
  
  for (int i = p; i < current_size-1; i++)
  {
    data[i] = data[i+1];
  }
  current_size--;
}

void TSCvector::removeLastElement()
{
  if (current_size > 0)
  {
    current_size--;
  }
}

void TSCvector::removeAllElements()
{
  current_size = 0;
}

void TSCvector::checkCapacity()
{
  if (current_capacity == current_size)
  {
    ensureCapacity(current_capacity*2);
  }
}
