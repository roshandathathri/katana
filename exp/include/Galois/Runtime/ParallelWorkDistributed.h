/** Galois Distributed Loop -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
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
 * @section Description
 *
 * Implementation of the Galois distributed foreach iterator.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#ifndef GALOIS_RUNTIME_PARALLELWORKDIST_H
#define GALOIS_RUNTIME_PARALLELWORKDIST_H

#include "Galois/Runtime/Network.h"

namespace Galois {
namespace ParallelSTL {
struct count_if_R : public Galois::Runtime::Lockable {
  ptrdiff_t i;
  count_if_R() :i(0) { }
  void add(ptrdiff_t v) {
    i += v;
    return;
  }
  typedef int tt_dir_blocking;
  // serialization functions
  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::Distributed::SerializeBuffer& s) const {
    gSerialize(s,i);
  }
  void deserialize(Galois::Runtime::Distributed::DeSerializeBuffer& s) {
    gDeserialize(s,i);
  }
};

template<typename Predicate>
struct count_if_helper_dist : public Galois::Runtime::Lockable {
  Predicate f;
  ptrdiff_t ret;
  count_if_helper_dist(): ret(0) { }
  count_if_helper_dist(Predicate p): f(p), ret(0) { }
  template<typename T>
  void operator()(const T& v) {
    if (f(v)) ++ret;
  }
  // serialization functions
  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::Distributed::SerializeBuffer& s) const {
    gSerialize(s,f);
  }
  void deserialize(Galois::Runtime::Distributed::DeSerializeBuffer& s) {
    gDeserialize(s,f);
  }
};

struct count_if_reducer_dist : public Galois::Runtime::Lockable {
  gptr<count_if_R> r;
  count_if_reducer_dist(count_if_R* _r = nullptr) :r(_r) {}
  template<typename CIH>
  void operator()(CIH& dest, const CIH& src) {
 //printf ("adding %ld and %ld in host %u and thread %u\n", dest.ret, src.ret, Runtime::Distributed::networkHostID, Runtime::LL::getTID());
    count_if_R* transient_r = r.transientAcquire();
    transient_r->add(dest.ret+src.ret);
    r.transientRelease();
  }
  // serialization functions
  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::Distributed::SerializeBuffer& s) const {
    gSerialize(s,r);
  }
  void deserialize(Galois::Runtime::Distributed::DeSerializeBuffer& s) {
    gDeserialize(s,r);
  }
};

}

namespace Runtime {

template<typename WLTy, typename ItemTy, typename FunctionTy>
void for_each_landing_pad(Distributed::RecvBuffer& buf) {
  //extract stuff
  FunctionTy f;
  std::deque<ItemTy> data;
  gDeserialize(buf,f,data);

  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //Start locally
  Galois::Runtime::for_each_impl<WLTy>(Galois::Runtime::makeStandardRange(data.begin(), data.end()), f, nullptr);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

template<typename WLTy, typename T, typename FunctionTy>
void for_each_local_landing_pad(Distributed::RecvBuffer& buf) {
  //extract stuff
  FunctionTy f;
  gptr<T> data;
  gDeserialize(buf,f,data);

  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //Start locally
  Galois::Runtime::for_each_impl<WLTy>(Galois::Runtime::makeLocalRange(*data), f, nullptr);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

template<typename T, typename FunctionTy, typename ReducerTy>
void do_all_impl_landing_pad(Distributed::RecvBuffer& buf) {
  //extract stuff
  FunctionTy f;
  ReducerTy  r;
  bool       needsReduce;
  gptr<T>    data;
  gDeserialize(buf,f,r,needsReduce,data);

  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //Start locally
  do_all_impl(Galois::Runtime::makeLocalRange(*data),f,r,needsReduce);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

template<typename FunctionTy>
void on_each_impl_landing_pad(Distributed::RecvBuffer& buf) {
  //extract stuff
  FunctionTy f;
  gDeserialize(buf,f);

  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //Start locally
  on_each_impl(f);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

namespace {

template<typename WLTy, typename IterTy, typename FunctionTy>
void for_each_dist(IterTy b, IterTy e, FunctionTy f, const char* loopname) {
  // Get a handle to the network interface
  //  Don't move as networkHostNum and networkHostID have to be initialized first
  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //fast path for non-distributed
  if (Distributed::networkHostNum == 1) {
    for_each_impl<WLTy>(Galois::Runtime::makeStandardRange(b, e),f,loopname);
    return;
  }

  typedef typename std::iterator_traits<IterTy>::value_type ItemTy;

  //copy out all data
  std::deque<ItemTy> allData;
  allData.insert(allData.end(), b,e);

  for (unsigned i = 1; i < Distributed::networkHostNum; i++) {
    auto blk = block_range(allData.begin(), allData.end(), i, Distributed::networkHostNum);
    std::deque<ItemTy> data(blk.first, blk.second);
    Distributed::SendBuffer buf;
    // serialize function and data
    gSerialize(buf,f,data);
    //send data
    net.sendMessage (i, &for_each_landing_pad<WLTy,ItemTy,FunctionTy>, buf);
  }
  net.handleReceives();
  //now get our data
  auto myblk = block_range(allData.begin(), allData.end(), 0, Distributed::networkHostNum);

  //Start locally
  for_each_impl<WLTy>(Galois::Runtime::makeStandardRange(myblk.first, myblk.second), f, loopname);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

template<typename WLTy, typename T, typename FunctionTy>
void for_each_local_dist(Runtime::Distributed::gptr<T>& c, FunctionTy f, const char* loopname) {
  // Get a handle to the network interface
  //  Don't move as networkHostNum and networkHostID have to be initialized first
  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //fast path for non-distributed
  if (Distributed::networkHostNum == 1) {
    for_each_impl<WLTy>(Galois::Runtime::makeLocalRange(*c),f,loopname);
    return;
  }

  for (unsigned i = 1; i < Distributed::networkHostNum; i++) {
    Distributed::SendBuffer buf;
    // serialize function and data
    gSerialize(buf,f,c);
    //send data
    net.sendMessage (i, &for_each_local_landing_pad<WLTy,T,FunctionTy>, buf);
  }
  net.handleReceives();
  //Start locally
  for_each_impl<WLTy>(Galois::Runtime::makeLocalRange(*c), f, loopname);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

template<typename T, typename FunctionTy, typename ReducerTy>
void do_all_impl_dist(Runtime::Distributed::gptr<T>& c, FunctionTy f, ReducerTy r, bool needsReduce) {
  // Get a handle to the network interface
  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  // Should be called only outside a for_each for now
  assert(!inGaloisForEach);

  //fast path for non-distributed
  if (Distributed::networkHostNum == 1) {
    do_all_impl(Galois::Runtime::makeLocalRange(*c),f,r,needsReduce);
    return;
  }

  for (unsigned i = 1; i < Distributed::networkHostNum; i++) {
    Distributed::SendBuffer buf;
    // serialize function and data
    gSerialize(buf,f,r,needsReduce,c);
    //send data
    net.sendMessage (i, &do_all_impl_landing_pad<T,FunctionTy,ReducerTy>, buf);
  }
  net.handleReceives();
  //Start locally
  do_all_impl(Galois::Runtime::makeLocalRange(*c),f,r,needsReduce);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

template<typename FunctionTy>
void on_each_impl_dist(FunctionTy f, const char* loopname) {
  // Get a handle to the network interface
  //  Don't move as networkHostNum and networkHostID have to be initialized first
  Distributed::NetworkInterface& net = Distributed::getSystemNetworkInterface();

  //fast path for non-distributed
  if (Distributed::networkHostNum == 1) {
    on_each_impl(f, loopname);
    return;
  }

  for (unsigned i = 1; i < Distributed::networkHostNum; i++) {
    Distributed::SendBuffer buf;
    // serialize function and data
    gSerialize(buf,f);
    //send data
    net.sendMessage (i, &on_each_impl_landing_pad<FunctionTy>, buf);
  }
  net.handleReceives();
  //Start locally
  on_each_impl(f, loopname);

  // place a MPI barrier here for all the hosts to synchronize
  net.systemBarrier();
}

} // anon
} // Runtime
} // Galois

#endif
