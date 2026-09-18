#include<globals.h>
#include<SRR_LSHDBSCAN.h>
#include<point.h>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace dbscan_srr {
namespace python {

namespace py = pybind11;

class SRR {
    std::unique_ptr<SRR_LSHDBSCAN> dbscan;
    dataset* ds;

public:

    Statistics c;

    std::unordered_map<std::string, double> statistics() {
        return c.stats;
    }

    
    std::vector<int> fit_predict(
        std::vector<std::vector<float>>& data,
        double delta,
        double mem_constraint,
        size_t numberOfThreads = 2,
        double epsilon_=1.0,
        int minPts_=100
    ) {
        std::vector<int> labels;
        ds = new dataset();
        ds->readData(data);

        epsilon_original = epsilon_;
        epsilon = epsilon_ * epsilon_;
        minPts = minPts_;

        dbscan = std::make_unique<SRR_LSHDBSCAN>(ds, delta, mem_constraint,
            numberOfThreads);


        dbscan->performClustering();

        for(auto p: ds->points){
            //std::cout << p.print() << std::endl;
            labels.push_back(p.print());
        }

        c = counters;


        return labels;

    }
};

PYBIND11_MODULE(dbscan_srr, m) {
    py::class_<SRR>(m, "SRR")
        .def(py::init<>())
        .def("fit_predict", &SRR::fit_predict)
        .def("statistics", &SRR::statistics);

}


} // namespace python
} // namespace puffinn


