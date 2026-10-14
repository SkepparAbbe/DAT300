// Tests for sort-based bucketing (A1).
//   sort_bucketing_test                      unit tests + synthetic end-to-end comparison (CTest)
//   sort_bucketing_test -f file [-a] -e eps -m minPts -L L -M M -t K
//                                            same comparison on a real dataset
// For every table, the hash-map buckets (baseline) and the CSR buckets (A1) must form the same
// partition, and the candidate buckets (>= minPts) must be the same sets.

#include <ConcurrentLSHDBSCAN.h>
#include <globals.h>
#include <sortBucketing.h>

#include <tbb/task_arena.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>

static void require(bool condition, const std::string& what)
{
  if (!condition)
    throw std::runtime_error("FAILED: " + what);
}

/* ---------------- radix sort + CSR ---------------- */

static void checkSort(const std::vector<uint64_t>& keys, size_t threads, unsigned angularBits)
{
  SortBuffers buffers;
  buffers.resize(keys.size());
  buffers.keys = keys;
  std::vector<std::pair<uint64_t, uint32_t>> expected;
  for (size_t i = 0; i < keys.size(); ++i)
    expected.emplace_back(keys[i], uint32_t(i));
  std::stable_sort(expected.begin(), expected.end(),
                   [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b)
                   { return a.first < b.first; });
  tbb::task_arena arena(static_cast<int>(threads));
  CsrTable table;
  arena.execute([&] { buffers.sort(threads, angularBits); table = buildCsr(buffers, threads); });

  for (size_t i = 0; i < keys.size(); ++i)
    require(buffers.keys[i] == expected[i].first && buffers.ids[i] == expected[i].second,
            "radix sort equals std::stable_sort");
  require(table.offsets.size() == table.keys.size() + 1 && table.offsets.front() == 0 &&
          table.offsets.back() == keys.size() && table.ids == buffers.ids, "CSR layout");
  for (size_t b = 0; b < table.keys.size(); ++b)
  {
    require(table.offsets[b] < table.offsets[b + 1], "CSR buckets are non-empty");
    require(b == 0 || table.keys[b - 1] < table.keys[b], "CSR bucket keys are distinct and sorted");
    for (size_t i = table.offsets[b]; i < table.offsets[b + 1]; ++i)
      require(buffers.keys[i] == table.keys[b], "CSR bucket members share the bucket key");
  }
}

static void unitTests()
{
  std::mt19937_64 random(42);
  for (size_t threads : {size_t(1), size_t(8), size_t(72)})
    for (size_t n : {size_t(0), size_t(1), size_t(7), size_t(1003)})
    {
      std::vector<uint64_t> keys(n);
      checkSort(keys, threads, 0);                                     // all equal
      for (auto& key : keys) key = random();
      checkSort(keys, threads, 0);                                     // all 8 digits vary
      std::sort(keys.begin(), keys.end());
      checkSort(keys, threads, 0);                                     // already sorted
      std::reverse(keys.begin(), keys.end());
      checkSort(keys, threads, 0);                                     // reversed
      for (auto& key : keys) key = (random() % 5) << 60;
      checkSort(keys, threads, 0);                                     // only the top digit varies
      for (auto& key : keys) key = random() % 17;
      checkSort(keys, threads, 0);                                     // many duplicates
      checkSort(keys, threads, 5);                                     // counting-sort fast path
      for (auto& key : keys) key = random() & 0xffff;
      checkSort(keys, threads, 16);                                    // fast path, largest M
    }
  std::cout << "radix sort + CSR: ok" << std::endl;
}

/* ---------------- baseline vs A1 inside ConcurrentLSHDBSCAN ---------------- */

typedef std::vector<std::vector<size_t>> Partition;

class Probe : public ConcurrentLSHDBSCAN
{
 public:
  Probe(dataset* ds, size_t L, size_t M, size_t threads, bool sortBucketing)
    : ConcurrentLSHDBSCAN(ds, L, M, threads, 100, false, sortBucketing) {}

  // Canonical bucket partition of table t (optionally only candidates): sorted ids, sorted buckets.
  Partition partition(size_t t, bool candidatesOnly) const
  {
    const HashTable& table = hashTables[t];
    Partition buckets;
    if (table.csr.offsets.empty())
    {
      for (const auto& entry : table.myMap)
        if (!candidatesOnly || entry.second.size() >= minPts)
        {
          buckets.push_back(std::vector<size_t>());
          for (const point* p : entry.second)
            buckets.back().push_back(p->id);
        }
    }
    else if (candidatesOnly)
      for (uint32_t b : table.candidateBuckets)
        buckets.push_back(std::vector<size_t>(table.csr.ids.begin() + table.csr.offsets[b],
                                              table.csr.ids.begin() + table.csr.offsets[b + 1]));
    else
      for (size_t b = 0; b < table.csr.keys.size(); ++b)
        buckets.push_back(std::vector<size_t>(table.csr.ids.begin() + table.csr.offsets[b],
                                              table.csr.ids.begin() + table.csr.offsets[b + 1]));
    for (auto& bucket : buckets)
      std::sort(bucket.begin(), bucket.end());
    std::sort(buckets.begin(), buckets.end());
    return buckets;
  }
  size_t tables() const { return hashTables.size(); }
};

static std::vector<long> labels(const dataset& ds)
{
  std::stringstream stream;
  ds.printData(stream, '\t', true);
  std::vector<long> result;
  long label;
  while (stream >> label)
    result.push_back(label);
  return result;
}

// Rand index between two labelings (noise -1 counts as one group, as in RandIndexCalculator).
static double randIndex(const std::vector<long>& a, const std::vector<long>& b)
{
  std::map<std::pair<long, long>, double> both;
  std::map<long, double> onlyA, onlyB;
  for (size_t i = 0; i < a.size(); ++i)
  {
    both[std::make_pair(a[i], b[i])] += 1;
    onlyA[a[i]] += 1;
    onlyB[b[i]] += 1;
  }
  auto pairs = [](double n) { return n * (n - 1) / 2; };
  double sumBoth = 0, sumA = 0, sumB = 0;
  for (const auto& cell : both) sumBoth += pairs(cell.second);
  for (const auto& cell : onlyA) sumA += pairs(cell.second);
  for (const auto& cell : onlyB) sumB += pairs(cell.second);
  const double total = pairs(double(a.size()));
  return total == 0 ? 1.0 : (total + 2 * sumBoth - sumA - sumB) / total;
}

// Runs the full clustering with both Phase I variants on the same data and seeds and compares them.
static void compareVariants(dataset& ds, const std::string& name, size_t L, size_t M, size_t threads)
{
  Probe baseline(&ds, L, M, threads, false);
  baseline.performClustering();
  const std::vector<long> baselineLabels = labels(ds);
  Probe sorted(&ds, L, M, threads, true);
  sorted.performClustering();
  const std::vector<long> sortedLabels = labels(ds);

  size_t buckets = 0, candidates = 0;
  for (size_t t = 0; t < L; ++t)
  {
    const Partition all = sorted.partition(t, false), selected = sorted.partition(t, true);
    require(baseline.partition(t, false) == all, name + ": identical bucket partition, table " + std::to_string(t));
    require(baseline.partition(t, true) == selected, name + ": identical candidate set, table " + std::to_string(t));
    buckets += all.size();
    candidates += selected.size();
  }
  std::cout << name << " L=" << L << " M=" << M << " K=" << threads << ": " << L
            << " tables identical, buckets=" << buckets << " candidates=" << candidates
            << ", Rand index of final labels (baseline vs A1) = " << randIndex(baselineLabels, sortedLabels)
            << std::endl;
}

static void setMetric(Metric selected, double eps)
{
  metric = selected;
  hashFunc = selected == angular ? angularHash : euclideanHash;
  distanceFunc = selected == angular ? angularDistance : squaredEuclideanDistance;
  epsilon_original = eps;
  epsilon = selected == angular ? eps : eps * eps;
}

static void syntheticTests()
{
  std::mt19937_64 random(7);
  std::normal_distribution<double> noise(0.0, 1.0);
  std::uniform_real_distribution<double> uniform(0.0, 100.0);
  dataset ds;
  ds.numberOfDimensions = 8;
  std::vector<std::vector<double>> centers(4, std::vector<double>(8));
  for (auto& center : centers)
    for (auto& value : center) value = uniform(random);
  for (size_t id = 0; id < 3000; ++id)
  {
    std::vector<double> features(8);
    for (size_t d = 0; d < 8; ++d)
      features[d] = id % 10 == 0 ? uniform(random) : centers[id % 4][d] + noise(random);  // 10% background
    ds.points.push_back(point(features));
    ds.points.back().id = id;
  }
  minPts = 20;
  for (size_t threads : {size_t(1), size_t(8)})
  {
    setMetric(euclidean, 3.0);
    compareVariants(ds, "synthetic euclidean", 6, 4, threads);
    dataset normalized = ds;
    normalized.normalizeData();
    setMetric(angular, 0.1);
    compareVariants(normalized, "synthetic angular", 6, 9, threads);
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc == 1)
    {
      unitTests();
      syntheticTests();
    }
    else
    {
      Parameters params;
      parseTheArguments(argc, argv, params);
      dataset ds;
      ds.readData(params.fileName);
      if (metric == angular)
        ds.normalizeData();
      compareVariants(ds, params.fileName, params.numberOfHashTables, params.numberOfHyperplanesPerTable,
                      params.numberOfThreads);
    }
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "PASS" << std::endl;
  return EXIT_SUCCESS;
}
