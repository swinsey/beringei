/**
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <folly/fibers/TimedMutex.h>
#include <bitset>
#include <vector>

#include "beringei/if/gen-cpp2/beringei_data_types.h"

namespace facebook {
namespace gorilla {

// The results of a BeringeiQuery.
// Values are returned in the same order as they were queried.
// Keys that were not found have empty result vectors.
//
// allSuccess is set to true if we were able to get a full copy of the results.
// memoryEstimate is an estimate of how much memory the query consumed, for the
// purposes of comparing the relative expense of different queries.
struct BeringeiGetResult {
  BeringeiGetResult() : allSuccess(false), memoryEstimate(0) {}
  explicit BeringeiGetResult(size_t size)
      : results(size), allSuccess(false), memoryEstimate(0) {}
  BeringeiGetResult(const BeringeiGetResult&) = delete;
  BeringeiGetResult& operator=(const BeringeiGetResult&) = delete;
  BeringeiGetResult(BeringeiGetResult&&) = default;
  BeringeiGetResult& operator=(BeringeiGetResult&&) = default;

  std::vector<std::vector<TimeValuePair>> results;
  bool allSuccess;
  size_t memoryEstimate;
};

// This class records results for a Beringei query as they arrive from multiple
// replicas of the service, tracking how much data was lost from each replica.
//
// Note: to do this quickly, it uses memory exponential in the number of
// replicas. As a typical setup is unlikely to have more than 3 replicas of the
// data, this is probably fine.
class BeringeiGetResultCollector {
 public:
  BeringeiGetResultCollector(
      size_t keys,
      size_t services,
      int64_t begin,
      int64_t end);

  // Insert data and return true if we just finished the first complete copy
  // of the results.
  bool addResults(
      const GetDataResult& results,
      const std::vector<size_t>& indices,
      size_t service);

  // Finalize data, record stats, and extract the result structure.
  // Throws an exception on incomplete results if requested to do so.
  // After this point, further calls to `addResults()` will be ignored.
  BeringeiGetResult finalize(
      bool validate,
      const std::vector<std::string>& serviceNames);

  // Use in tests only.
  const std::vector<int64_t>& getMismatchesForTesting() const {
    return mismatches_;
  }

 private:
  // Add results.
  void merge(size_t i, size_t service, const TimeSeriesData& result);

  // Begin and end time for the query to remove extraneous data.
  int64_t beginTime_, endTime_;

  // How many copies we're expecting for each key.
  size_t numServices_;

  // How many keys have no results.
  size_t remainingKeys_;

  // Which services have reported which keys.
  struct KeyStats {
    uint32_t count;
    std::bitset<32> received;
  };
  std::vector<KeyStats> complete_;

  // How much data was missing from each service.
  std::vector<int> drops_;

  // Mismatches per each service's first merge, indexed by 1ull << service.
  std::vector<int64_t> mismatches_;

  folly::fibers::TimedMutex lock_;
  bool done_;
  BeringeiGetResult result_;
};
}
}
