#ifndef __VANILLA_DBSCAN__
#define __VANILLA_DBSCAN__

#include <globals.h>
#include <point.h>

void VanillaDBSCAN(dataset *);
std::vector<point *> getEpsNeighbours(std::vector<point> &, point &);

#endif
