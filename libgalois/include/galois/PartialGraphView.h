#ifndef GALOIS_LIBGALOIS_GALOIS_PARTIAL_GRAPH_VIEW_H_
#define GALOIS_LIBGALOIS_GALOIS_PARTIAL_GRAPH_VIEW_H_

#include <fcntl.h>
#include <cstdio>
#include <memory>

#include <boost/iterator/counting_iterator.hpp>

#include "galois/OutIndexView.h"
#include "galois/graphs/GraphHelpers.h"
#include "galois/Result.h"
#include "tsuba/FileView.h"

namespace galois {

template <typename EdgeData, typename Edge>
class PartialGraphView {
  OutIndexView outidx_;
  tsuba::FileView edge_file_;
  tsuba::FileView edge_data_file_;
  uint64_t first_node_;
  uint64_t last_node_;
  const Edge* edges_;
  const EdgeData* edge_data_;
  uint64_t n_edges_;

  galois::Result<void> BindEdgeFile() {
    sz_t edges_offset = outidx_.view_size();
    sz_t edges_start  = edges_offset + (*edge_begin() * sizeof(Edge));
    sz_t edges_stop   = edges_offset + (*edge_end() * sizeof(Edge));
    if (auto res = edge_file_.Bind(outidx_.filename(), edges_start, edges_stop);
        !res) {
      return res.error();
    }
    return galois::ResultSuccess();
  }

  template <typename T>
  size_t AlignUp(size_t val) {
    return (val + (sizeof(T) - 1)) & ~(sizeof(T) - 1);
  }

  template <
      typename EdgeType,
      typename std::enable_if<!std::is_void<EdgeType>::value>::type* = nullptr>
  galois::Result<void> BindEdgeDataFile() {
    /* index past all of the edges */
    sz_t edge_data_offset =
        outidx_.view_size() + (outidx_.num_edges() * sizeof(Edge));
    if constexpr (sizeof(Edge) < sizeof(uint64_t)) {
      edge_data_offset = AlignUp<uint64_t>(edge_data_offset);
    }
    sz_t data_start = edge_data_offset + (*edge_begin() * sizeof(EdgeData));
    sz_t data_stop  = edge_data_offset + (*edge_end() * sizeof(EdgeData));
    if (auto res =
            edge_data_file_.Bind(outidx_.filename(), data_start, data_stop);
        !res) {
      return res.error();
    }
    return galois::ResultSuccess();
  }

  template <
      typename EdgeType,
      typename std::enable_if<std::is_void<EdgeType>::value>::type* = nullptr>
  galois::Result<void> BindEdgeDataFile() {
    return galois::ResultSuccess();
  }

public:
  typedef boost::counting_iterator<uint64_t> edge_iterator;
  typedef boost::counting_iterator<uint64_t> iterator;

  PartialGraphView(const PartialGraphView&) = delete;
  PartialGraphView& operator=(const PartialGraphView&) = delete;

  PartialGraphView(OutIndexView&& outidx) : outidx_(std::move(outidx)) {}

  PartialGraphView(PartialGraphView&& other) noexcept = default;
  PartialGraphView& operator=(PartialGraphView&& other) noexcept = default;
  ~PartialGraphView() { Unbind(); }

  galois::Result<void> Bind(index_t node_begin, index_t node_end) {
    first_node_ = node_begin;
    last_node_  = node_end;
    if (auto res = BindEdgeFile(); !res) {
      return res.error();
    }
    if (auto res = BindEdgeDataFile<EdgeData>(); !res) {
      if (auto res = edge_file_.Unbind(); !res) {
        return res.error();
      }
      return res.error();
    }
    edges_     = edge_file_.ptr<Edge>();
    edge_data_ = edge_data_file_.ptr<EdgeData>();
    return galois::ResultSuccess();
  }

  void Unbind() {
    if (auto res = outidx_.Unbind(); !res) {
      GALOIS_LOG_ERROR("outindx_.Unbind(): {}", res.error());
    }
    if (auto res = edge_file_.Unbind(); !res) {
      GALOIS_LOG_ERROR("edge_file_.Unbind(): {}", res.error());
    }
    if (auto res = edge_data_file_.Unbind(); !res) {
      GALOIS_LOG_ERROR("edge_data_file_.Unbind(): {}", res.error());
    }
    first_node_ = 0;
    last_node_  = 0;
    edges_      = nullptr;
    edge_data_  = nullptr;
    n_edges_    = 0;
  }

  const OutIndexView& out_index() const { return outidx_; }
  iterator node_begin() const { return iterator(first_node_); }
  iterator node_end() const { return iterator(last_node_); }
  edge_iterator edge_begin() const { return outidx_.edge_begin(first_node_); }
  edge_iterator edge_end() const { return outidx_.edge_begin(last_node_); }
  const Edge* edges() const { return edges_; }
  const EdgeData* edge_data() const { return edge_data_; }
};

} /* namespace galois */

#endif
