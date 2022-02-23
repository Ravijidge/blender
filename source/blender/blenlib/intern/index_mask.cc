/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask.hh"

namespace blender {

IndexMask IndexMask::slice(int64_t start, int64_t size) const
{
  return this->slice(IndexRange(start, size));
}

IndexMask IndexMask::slice(IndexRange slice) const
{
  return IndexMask(indices_.slice(slice));
}

IndexMask IndexMask::slice_and_offset(const IndexRange slice, Vector<int64_t> &r_new_indices) const
{
  const int slice_size = slice.size();
  if (slice_size == 0) {
    return {};
  }
  IndexMask sliced_mask{indices_.slice(slice)};
  if (sliced_mask.is_range()) {
    return IndexMask(slice_size);
  }
  const int64_t offset = sliced_mask.indices().first();
  if (offset == 0) {
    return sliced_mask;
  }
  r_new_indices.resize(slice_size);
  for (const int i : IndexRange(slice_size)) {
    r_new_indices[i] = sliced_mask[i] - offset;
  }
  return IndexMask(r_new_indices.as_span());
}

IndexMask IndexMask::invert(const IndexRange full_range, Vector<int64_t> &r_new_indices) const
{
  BLI_assert(this->contained_in(full_range));
  if (full_range.size() == indices_.size()) {
    return {};
  }
  if (indices_.is_empty()) {
    return full_range;
  }
  r_new_indices.clear();

  const Vector<IndexRange> ranges = this->extract_ranges_invert(full_range, nullptr);
  for (const IndexRange &range : ranges) {
    for (const int64_t index : range) {
      r_new_indices.append(index);
    }
  }
  return r_new_indices.as_span();
}

Vector<IndexRange> IndexMask::extract_ranges() const
{
  Vector<IndexRange> ranges;
  int64_t range_start = 0;
  while (range_start < indices_.size()) {
    int64_t current_range_end = range_start + 1;
    int64_t step_size = 1;

    while (true) {
      const int64_t possible_range_end = current_range_end + step_size;
      if (possible_range_end > indices_.size()) {
        break;
      }
      if (!this->slice(range_start, possible_range_end - range_start).is_range()) {
        break;
      }
      current_range_end = possible_range_end;
      step_size *= 2;
    }

    /* This step size was tried already, no need to try it again. */
    step_size /= 2;

    while (step_size > 0) {
      const int64_t possible_range_end = current_range_end + step_size;
      step_size /= 2;
      if (possible_range_end > indices_.size()) {
        continue;
      }
      if (!this->slice(range_start, possible_range_end - range_start).is_range()) {
        continue;
      }
      current_range_end = possible_range_end;
    }

    ranges.append(IndexRange{indices_[range_start], current_range_end - range_start});
    range_start = current_range_end;
  }
  return ranges;
}

Vector<IndexRange> IndexMask::extract_ranges_invert(const IndexRange full_range,
                                                    Vector<int64_t> *r_skip_amounts) const
{
  BLI_assert(this->contained_in(full_range));
  const Vector<IndexRange> ranges = this->extract_ranges();
  Vector<IndexRange> inverted_ranges;

  int64_t skip_amount = 0;
  int64_t next_start = full_range.start();
  for (const int64_t i : ranges.index_range()) {
    const IndexRange range = ranges[i];
    if (range.start() > next_start) {
      inverted_ranges.append({next_start, range.start() - next_start});
      if (r_skip_amounts != nullptr) {
        r_skip_amounts->append(skip_amount);
      }
    }
    next_start = range.one_after_last();
    skip_amount += range.size();
  }
  if (next_start < full_range.one_after_last()) {
    inverted_ranges.append({next_start, full_range.one_after_last() - next_start});
    if (r_skip_amounts != nullptr) {
      r_skip_amounts->append(skip_amount);
    }
  }
  return inverted_ranges;
}

}  // namespace blender
