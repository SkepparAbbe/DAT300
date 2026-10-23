#ifndef __TRANSFORMATION_H__
#define __TRANSFORMATION_H__

#include "tbb/concurrent_vector.h"
#include <point.h>
#include <randomGen.h>
#include <unordered_map>

template <class T> inline void hash_combine(std::size_t &seed, const T &v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

class MyHasher {
public:
  size_t operator()(HashedPoint const &p) const noexcept {
    std::size_t seed = 0;

    for (const auto &elem : p.features)
      hash_combine(seed, std::hash<int>()(elem));

    //    std::size_t h = std::hash<std::string>() (strRep);

    return seed;
  }
};

/*
class MyHasher
{
 public:
  size_t operator() (HashedPoint const & p) const noexcept
  {
    std::string strRep = "";

    for (const auto & elem : p.features)
      strRep +=std::to_string((int)elem);
    //    std::cout << strRep << std::endl;

    std::size_t h = std::hash<std::string>() (strRep);

    return h;
  }
};
*/

class CoreBucket {
public:
  std::vector<point *> members;
  point *representative = NULL;
};

class HashTable {
private:
  dataset *ds = NULL;
  RandGenerator *gen;

  void initializeHashTable(size_t);
  void initializeHashTable(std::string &);

  std::ostream &print(std::ostream &stream, char deli) const;

  void identifyMergeTasks();
  size_t numberOfHyperplanes;
  size_t numberOfThreads = 1;

public:
  HashTable(dataset *, size_t, RandGenerator *, size_t numberOfThreads_ = 1);
  HashTable(dataset *, std::string &, size_t numberOfThreads_ = 1);

  std::vector<Hyperplane> hyperplanes;
  std::vector<std::unordered_map<HashedPoint, std::vector<point *>, MyHasher>>
      threadMaps;
  std::unordered_map<HashedPoint, std::vector<const std::vector<point *> *>,
                     MyHasher>
      bucketIndex;

  std::vector<CoreBucket> coreBuckets;
  std::vector<std::pair<point *, point *>> mergeTasks;

  void populateHashTable();
  void identifyCoreBuckets();
  void identifyCoreBuckets_densityStyle();
  void identifyMergeTasks_V2();
  void identifyMergeTasks_V2(
      tbb::concurrent_vector<std::pair<std::pair<point *, point *>, bool>> &);
  void identifyAndPerformMergeTasks();
  void performMergeTasks();
  size_t getMedian(std::vector<point *> &) const;

  void populateHashTable(std::vector<point>::iterator,
                         std::vector<point>::iterator, size_t threadID);
  void buildBucketIndex();
  std::vector<point *> collectPoints(const HashedPoint &key) const;
  size_t getClosestToMean(std::vector<point *> &vec) const;

  std::ostream &printTable(std::ostream &stream, char deli) const;
  std::ostream &printCoreBuckets(std::ostream &, char) const;
  std::ostream &printMergeTasks(std::ostream &stream, char deli) const;

  std::vector<point *>
  getEpsNeighbours(point &); /* used in ValillaDBSCAN+LSH */
  std::vector<point *>
  getEpsNeighbours(point &, HashedPoint &); /* used in ValillaDBSCAN+LSH */
};

#endif
