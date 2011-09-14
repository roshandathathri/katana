/** Simple thread related classes -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef GALOIS_RUNTIME_THREADS_H
#define GALOIS_RUNTIME_THREADS_H

#include <vector>

#include "Galois/Runtime/Config.h"

namespace GaloisRuntime {

struct runCMD {
  config::function<void (void)> work;
  bool isParallel;
  bool barrierAfter;
};

class ThreadPool {
protected:
  static __thread unsigned int LocalThreadID;
  unsigned int activeThreads;

public:

  //!execute work on all threads
  //!preWork and postWork are executed only on the master thread
  virtual void run(runCMD* begin, runCMD* end) = 0;
  
  //!change the number of threads to num
  //!returns the number that the runtime chooses (may not be num)
  virtual unsigned int setActiveThreads(unsigned int num) = 0;

  //!How many threads will be used
  unsigned int getActiveThreads() const { return activeThreads; }

  //!My thread id (dense, user thread is 0, galois threads 1..num)
  static unsigned int getMyID() { return LocalThreadID; }

};

//Returns or creates the appropriate thread pool for the system
ThreadPool& getSystemThreadPool();

class ThreadPolicy {
protected:
  //num levels
  int numLevels;
  
  //number of hw supported threads
  int numThreads;
  
  //number of "real" processors
  int numCores;

  //example levels:
  //thread(0), Cpu(1), numa(2), machine(3)

  //Total number of threads in each level
  std::vector<int> levelSize;

  //[numLevels][numThreads] -> item index for thread at level
  std::vector<int> levelMap;

public:
  int indexLevelMap(int level, int thr) const {
    return levelMap[level * numThreads + thr];
  }

  int getNumLevels() const { return numLevels; }

  int getNumThreads() const { return numThreads; }

  int getNumCores() const { return numCores; }

  int getLevelSize(int S) const { return levelSize[S]; }

  int isFirstInLevel(int level, int thr) const {
    int thrLevel = indexLevelMap(level, thr);
    for (int i = 0; i < getNumThreads(); ++i)
      if (indexLevelMap(level, i) == thrLevel)
	return i == thr;
    //Should be dead:
    return false;
  }

  virtual void bindThreadToProcessor(int id) = 0;
};

ThreadPolicy& getSystemThreadPolicy();

}

#endif
