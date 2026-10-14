#include <ConcurrentLSHDBSCAN.h>
#include <cassert>
#include <iostream>
#include <pthread.h>
#include <globals.h>
#include <stdexcept>
#include <tbb/task_arena.h>
ConcurrentLSHDBSCAN::ConcurrentLSHDBSCAN(dataset* ds_,
					 size_t numberOfHashTables_,
					 size_t numberOfHyperplanesPerTable_,
					 size_t numberOfThreads_,
					 size_t numberOfBatches_,
					 bool benchamrk_,
					 bool sortBucketing_) :
  numberOfThreads((assert(numberOfThreads_>=1), numberOfThreads_)),
  numberOfBatches((assert(numberOfBatches_>=1), numberOfBatches_)),
  sortBucketing(sortBucketing_),
  LSHDBSCAN(ds_,
	    numberOfHashTables_,
	    numberOfHyperplanesPerTable_,
	    benchamrk_)
{
  pid = new pthread_t[numberOfThreads];

  populationTasks.reserve(numberOfBatches*numberOfHashTables);
  relabelingTasks.reserve(numberOfBatches);
  coreBucketIdentificationTasks.reserve(numberOfHashTables);
  mergeTasksIdentificationTasks.reserve(numberOfHashTables);
  
  std::vector<point>::iterator begin, end;
  size_t batchSize = round(this->ds->points.size()/numberOfBatches);
    
  begin = this->ds->points.begin();
  end = begin + batchSize;
  
  for (size_t batch = 0; batch < numberOfBatches; ++batch)

    {
      if ( batch == numberOfBatches - 1)
	end = this->ds->points.end();
      
      for ( size_t table = 0; table < numberOfHashTables; ++table)
	{
	  populationTasks.emplace_back(begin, end, hashTables[table]);
	}
      relabelingTasks.emplace_back(begin, end);
      begin += batchSize;
      end += batchSize;
    }

  for ( size_t table = 0; table < numberOfHashTables; ++table)
    {
      coreBucketIdentificationTasks.emplace_back(hashTables[table]);
      mergeTasksIdentificationTasks.emplace_back(hashTables[table]);
    }
  assert(populationTasks.size() == numberOfBatches*numberOfHashTables);
}

/* Phase I, A1: for each table, key every point, radix-sort (key, id) and store the table as CSR;
   candidate buckets are the runs with >= minPts members. Sort buffers are reused across tables.
   Keys are computed point by point for a group of tables, so each point is read from memory once
   per group rather than once per table; the group's keys take at most ~128 MB. */
void ConcurrentLSHDBSCAN::populateSortedTables()
{
  const size_t N = ds->points.size();
  if (N >= (size_t(1) << 32))
    throw std::invalid_argument("Sort bucketing uses 32-bit point ids");
  if (metric == angular && numberOfHyperplanesPerTable > 64)
    throw std::invalid_argument("Angular sort bucketing requires M <= 64");

  const size_t L = hashTables.size();
  const size_t group = std::max<size_t>(1, std::min(L, (size_t(128) << 20) / (sizeof(uint64_t) * std::max<size_t>(N, 1))));
  std::vector<std::vector<uint64_t>> groupKeys(group, std::vector<uint64_t>(N));
  SortBuffers buffers;
  buffers.resize(N);
  tbb::task_arena arena(static_cast<int>(numberOfThreads));
  arena.execute([&]
  {
    for (size_t first = 0; first < L; first += group)
      {
	const size_t count = std::min(group, L - first);
	tbb::parallel_for(size_t(0), N, [&](size_t i)
	{
	  for (size_t g = 0; g < count; ++g)
	    groupKeys[g][i] = hashTables[first + g].bucketKey(ds->points[i]);
	});
	for (size_t g = 0; g < count; ++g)
	  {
	    HashTable & table = hashTables[first + g];
	    buffers.keys.swap(groupKeys[g]);
	    buffers.sort(numberOfThreads, metric == angular ? unsigned(numberOfHyperplanesPerTable) : 0);
	    table.csr = buildCsr(buffers, numberOfThreads);
	    table.candidateBuckets.clear();
	    for (uint32_t bucket = 0; bucket < table.csr.keys.size(); ++bucket)
	      if (table.csr.offsets[bucket + 1] - table.csr.offsets[bucket] >= minPts)
		table.candidateBuckets.push_back(bucket);
	  }
      }
  });
}

void ConcurrentLSHDBSCAN::populateHashTables()
{
  if (sortBucketing)
    {
      std::cout << "Populating with sort-based bucketing!" << std::endl;
      populateSortedTables();
      return;
    }
  std::cout << "Populating the concurrent way!" << std::endl;

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_create(&pid[threadID], NULL, populateHashTables_thread, this);
    }

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_join(pid[threadID], NULL);
    }
}

void ConcurrentLSHDBSCAN::identifyCoreBuckets()
{
  std::cout << "Identifying core-buckets the concurrent way!" << std::endl;

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_create(&pid[threadID], NULL, identifyCoreBuckets_thread, this);
    }

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_join(pid[threadID], NULL);
    }
}

void ConcurrentLSHDBSCAN::identifyMergeTasks()
{
  std::cout << "Identifying and Performing the merge tasks the concurrent way!" << std::endl;

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_create(&pid[threadID], NULL, identifyAndPerformMergeTasks_thread, this);
      //pthread_create(&pid[threadID], NULL, identifyMergeTasks_thread, this);
    }

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_join(pid[threadID], NULL);
    }

  //std::cout << "mergeTasks.size(): " << mergeTasks.size() << std::endl;
}

void ConcurrentLSHDBSCAN::performMergeTasks()
{
  return ; //Remove return if mergeTasks are not performed in function identifyMergeTasks()


  std::cout << "Performing the mergeTasks the concurrent way!" << std::endl;

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_create(&pid[threadID], NULL, performMergeTasks_thread, this);
    }

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_join(pid[threadID], NULL);
    }
}

void ConcurrentLSHDBSCAN::performRelabeling()
{
  std::cout << "Performing concurrent relabeling!" << std::endl;

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_create(&pid[threadID], NULL, performRelabelingTasks_thread, this);
    }

  for (size_t threadID = 0; threadID < numberOfThreads; ++threadID)
    {
      pthread_join(pid[threadID], NULL);
    }
}

void* ConcurrentLSHDBSCAN::populateHashTables_thread(void *inputArg)
{
  auto input = (ConcurrentLSHDBSCAN*) inputArg;
  
  for ( auto & task : input->populationTasks)
    {
      if (task.booked == false)
	{
	  if (__sync_bool_compare_and_swap ( &(task.booked),
					     false,
					     true)
	      )
	    {
	      task.table.populateHashTable(task.begin, task.end);
	    }
	}
    }  
  return NULL;
}

void* ConcurrentLSHDBSCAN::identifyCoreBuckets_thread(void *inputArg)
{
  auto input = (ConcurrentLSHDBSCAN*) inputArg;
  
  for ( auto & task : input->coreBucketIdentificationTasks)
    {
      if (task.booked == false)
	{
	  if (__sync_bool_compare_and_swap ( &(task.booked),
					     false,
					     true)
	      )
	    {
		if (metric == euclidean)
			task.table.identifyCoreBuckets_densityStyle();
		else
			task.table.identifyCoreBuckets();
	    }
	}
    }  
  return NULL;
}

void* ConcurrentLSHDBSCAN::identifyAndPerformMergeTasks_thread(void *inputArg)
{
  auto input = (ConcurrentLSHDBSCAN*) inputArg;
  
  for ( auto & task : input->mergeTasksIdentificationTasks)
    {
      if (task.booked == false)
	{
	  if (__sync_bool_compare_and_swap ( &(task.booked),
					     false,
					     true)
	      )
	    {
	      //task.table.identifyMergeTasks_V2(input->mergeTasks);
	      task.table.identifyAndPerformMergeTasks();
	    }
	}
    }  
  return NULL;
}

void* ConcurrentLSHDBSCAN::performMergeTasks_thread(void *inputArg)
{
  ConcurrentLSHDBSCAN* input = (ConcurrentLSHDBSCAN*) inputArg;

  for (auto & task : input->mergeTasks)
    {
      if(task.second == false)
	{
	  if (__sync_bool_compare_and_swap ( &(task.second),
					     false,
					     true)
	      )
	    {
	      task.first.first->link(task.first.second);
	    }
	}
    }

  return NULL;
}

void* ConcurrentLSHDBSCAN::performRelabelingTasks_thread(void *inputArg)
{
  auto input = (ConcurrentLSHDBSCAN*) inputArg;
  
  for ( auto & task : input->relabelingTasks)
    {
      if (task.booked == false)
	{
	  if (__sync_bool_compare_and_swap ( &(task.booked),
					     false,
					     true)
	      )
	    {
	      for (auto iter = task.begin; iter < task.end; iter++)
		{
		  iter->reLabel();
		}
	    }
	}
    }  
  return NULL;
}


void ConcurrentLSHDBSCAN::introduceMe() const
{
  std::cout << "Concurrent LSHDBSCAN:\n"
	    << "\t#points: " << this->ds->points.size()
	    << "\t#dims: " << this->ds->numberOfDimensions
    	    << "\tmetric: " << ( (metric == angular) ? "angular" : "euclidean")
	    << std::endl
	    << "\t#HashTables: " << numberOfHashTables
	    << "\t#HyperPlanesPerHashTable: " << numberOfHyperplanesPerTable
	    << "\t#threads: " << numberOfThreads
	    << "\t#batches: " << numberOfBatches
	    << "\tbucketing: " << (sortBucketing ? "sort (A1)" : "hash maps")
	    << std::endl
	    << "\tEps: " << epsilon_original
	    << "\tminPts: " << minPts
	    << std::endl;
}
