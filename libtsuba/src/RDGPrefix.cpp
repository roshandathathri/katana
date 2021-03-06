#include "tsuba/RDGPrefix.h"

#include "RDGHandleImpl.h"
#include "RDGPartHeader.h"
#include "galois/Result.h"
#include "tsuba/Errors.h"
#include "tsuba/file.h"

namespace tsuba {

galois::Result<tsuba::RDGPrefix>
RDGPrefix::DoMakePrefix(const tsuba::RDGMeta& meta) {
  auto meta_res = RDGPartHeader::Make(meta.PartitionFileName(0));
  if (!meta_res) {
    return meta_res.error();
  }

  tsuba::RDGPartHeader part_header = std::move(meta_res.value());
  if (part_header.topology_path().empty()) {
    return RDGPrefix{};
  }

  galois::Uri t_path = meta.dir().Join(part_header.topology_path());

  RDGPrefix::GRHeader gr_header;
  if (auto res = FileGet(t_path.string(), &gr_header); !res) {
    GALOIS_LOG_DEBUG(
        "file get failed: {}: sz: {}: {}", t_path, sizeof(gr_header),
        res.error());
    return res.error();
  }
  FileView fv;
  if (auto res = fv.Bind(
          t_path.string(),
          sizeof(gr_header) + (gr_header.num_nodes * sizeof(uint64_t)), true);
      !res) {
    GALOIS_LOG_DEBUG("FileView bind failed: {}: {}", t_path, res.error());
    return res.error();
  }

  return RDGPrefix(
      std::move(fv),
      sizeof(gr_header) + (gr_header.num_nodes * sizeof(uint64_t)));
}

galois::Result<tsuba::RDGPrefix>
RDGPrefix::Make(RDGHandle handle) {
  if (handle.impl_->rdg_meta().num_hosts() != 1) {
    GALOIS_LOG_ERROR("cannot construct RDGPrefix for partitioned graph");
    return ErrorCode::NotImplemented;
  }

  return DoMakePrefix(handle.impl_->rdg_meta());
}

}  // namespace tsuba
