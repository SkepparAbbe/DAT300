#ifndef __SORT_BUCKETING_H__
#define __SORT_BUCKETING_H__

// Sort-based bucketing (A1) for Phase I: pack each point's M hash values into a uint64 key,
// radix-sort (key, id) in parallel and store the table in CSR layout.

#include <globals.h>

#include <tbb/parallel_for.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

inline uint64_t splitmix64(uint64_t value)
{
  value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
  return value ^ (value >> 31);
}

// Adds hash value m to the key. Angular: sign bit m (M <= 64).
// Euclidean: splitmix64-style mix; distinct tuples collide with probability ~ B^2 / 2^65 per table.
inline uint64_t packKeyStep(uint64_t key, int value, size_t m)
{
  if (metric == angular)
    return key | (uint64_t(value > 0) << m);
  return splitmix64(key ^ splitmix64(uint64_t(int64_t(value)) + UINT64_C(0x9e3779b97f4a7c15) + m));
}

inline uint64_t packKey(const int* values, size_t M)
{
  uint64_t key = 0;
  for (size_t m = 0; m < M; ++m)
    key = packKeyStep(key, values[m], m);
  return key;
}

struct CsrTable
{
  std::vector<uint32_t> ids, offsets;  // ids grouped by bucket; bucket b = ids[offsets[b], offsets[b+1])
  std::vector<uint64_t> keys;          // key of each distinct bucket
  size_t bytes() const
  {
    return ids.capacity() * sizeof(uint32_t) + offsets.capacity() * sizeof(uint32_t) +
           keys.capacity() * sizeof(uint64_t);
  }
};

// Parallel stable LSD radix sort of (key, id); buffers are reused across tables.
// Call inside a tbb::task_arena sized to the wanted number of threads.
struct SortBuffers
{
  std::vector<uint64_t> keys, spareKeys;
  std::vector<uint32_t> ids, spareIds;
  std::vector<size_t> counts;
  std::vector<uint64_t> differs;

  void resize(size_t n) { keys.resize(n); spareKeys.resize(n); ids.resize(n); spareIds.resize(n); }

  // One counting pass on digit (key >> shift) & (bins - 1): per-thread histograms,
  // prefix sum over (digit, thread), then a disjoint stable scatter.
  void pass(size_t threads, unsigned shift, size_t bins)
  {
    const size_t n = keys.size();
    // Bounds are hoisted into locals: stores through size_t/uint64_t pointers may alias n.
    auto begin = [&](size_t t) { return n * t / threads; };
    counts.assign(threads * bins, 0);
    tbb::parallel_for(size_t(0), threads, [&](size_t t)
    {
      size_t* histogram = &counts[t * bins];
      for (size_t i = begin(t), end = begin(t + 1); i < end; ++i)
        ++histogram[(keys[i] >> shift) & (bins - 1)];
    });
    size_t offset = 0;
    for (size_t digit = 0; digit < bins; ++digit)
      for (size_t t = 0; t < threads; ++t)
      {
        const size_t count = counts[t * bins + digit];
        counts[t * bins + digit] = offset;
        offset += count;
      }
    tbb::parallel_for(size_t(0), threads, [&](size_t t)
    {
      size_t* position = &counts[t * bins];
      for (size_t i = begin(t), end = begin(t + 1); i < end; ++i)
      {
        const size_t destination = position[(keys[i] >> shift) & (bins - 1)]++;
        spareKeys[destination] = keys[i];
        spareIds[destination] = ids[i];
      }
    });
    keys.swap(spareKeys);
    ids.swap(spareIds);
  }

  // keys must be filled; ids are set to 0..n-1. angularBits = M for angular keys, 0 otherwise.
  void sort(size_t threads, unsigned angularBits)
  {
    const size_t n = keys.size();
    std::iota(ids.begin(), ids.end(), uint32_t(0));
    if (n < 2)
      return;
    threads = std::min(threads, n);
    if (angularBits != 0 && angularBits <= 16)  // fast path: one counting sort over 2^M bins
    {
      pass(threads, 0, size_t(1) << angularBits);
      return;
    }
    differs.assign(threads, 0);  // bits that differ from keys[0] somewhere
    tbb::parallel_for(size_t(0), threads, [&](size_t t)
    {
      uint64_t bits = 0;
      for (size_t i = n * t / threads, end = n * (t + 1) / threads; i < end; ++i)
        bits |= keys[i] ^ keys[0];
      differs[t] = bits;
    });
    const uint64_t varying = std::accumulate(differs.begin(), differs.end(), uint64_t(0),
                                             [](uint64_t a, uint64_t b) { return a | b; });
    for (unsigned shift = 0; shift < 64; shift += 8)
      if ((varying >> shift) & 0xff)  // skip 8-bit digits identical across all keys
        pass(threads, shift, 256);
  }
};

// One scan over the sorted keys marks bucket starts -> offsets and keys; ids copied out.
// The scan is split over threads (count starts, prefix sum, write) so it does not serialize.
inline CsrTable buildCsr(const SortBuffers& sorted, size_t threads)
{
  const size_t n = sorted.keys.size();
  threads = std::max<size_t>(1, std::min(threads, n));
  auto begin = [&](size_t t) { return n * t / threads; };
  auto isStart = [&](size_t i) { return i == 0 || sorted.keys[i] != sorted.keys[i - 1]; };
  std::vector<size_t> firstBucket(threads + 1, 0);
  tbb::parallel_for(size_t(0), threads, [&](size_t t)
  {
    size_t starts = 0;
    for (size_t i = begin(t), end = begin(t + 1); i < end; ++i)
      starts += isStart(i);
    firstBucket[t + 1] = starts;
  });
  std::partial_sum(firstBucket.begin(), firstBucket.end(), firstBucket.begin());
  CsrTable table;
  table.ids.resize(n);
  table.offsets.resize(firstBucket.back() + 1);
  table.keys.resize(firstBucket.back());
  tbb::parallel_for(size_t(0), threads, [&](size_t t)
  {
    size_t bucket = firstBucket[t];
    for (size_t i = begin(t), end = begin(t + 1); i < end; ++i)
    {
      table.ids[i] = sorted.ids[i];
      if (isStart(i))
      {
        table.offsets[bucket] = uint32_t(i);
        table.keys[bucket++] = sorted.keys[i];
      }
    }
  });
  table.offsets.back() = uint32_t(n);
  return table;
}

#endif
