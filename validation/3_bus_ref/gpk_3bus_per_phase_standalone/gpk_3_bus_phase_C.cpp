#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include "gridpack/include/gridpack.hpp"
#include "pf_app.hpp"

static std::vector<std::string> build_timestamps_24h()
{
    std::vector<std::string> ts;

    for (int h = 0; h < 24; ++h) {
        for (int m = 0; m < 60; m += 15) {
            std::ostringstream oss;
            oss << "2000-09-01 "
                << std::setw(2) << std::setfill('0') << h << ":"
                << std::setw(2) << std::setfill('0') << m << ":00";
            ts.push_back(oss.str());
        }
    }

    ts.push_back("2000-09-02 00:00:00");
    return ts;
}

int main(int argc, char** argv)
{
    gridpack::Environment env(argc, argv);
    gridpack::math::Initialize(&argc, &argv);

    const char* xmlfile = "../input_3_bus_phase_C.xml";
    std::vector<std::string> timestamps = build_timestamps_24h();

    {
        std::ofstream clr("gpk_timeseries_phaseC.csv");
    }

    for (std::size_t k = 0; k < timestamps.size(); ++k) {
        gridpack::powerflow::PFApp app;

        char* argv2[2];
        argv2[0] = argv[0];
        argv2[1] = const_cast<char*>(xmlfile);

        app.execute(2, argv2);

        if (k == 0) {
            app.writeBusVoltagesCSV("bus_voltages_phaseC_standalone.csv");
        }

        app.appendBusVoltagesTimeSeriesCSV("gpk_timeseries_phaseC.csv", timestamps[k], "C");

        double V = app.getBoundaryMagnitude();
        double P = app.getBoundaryPinjection();

        std::cout << "[Phase C][" << timestamps[k]
            << "] Boundary V = " << V
            << " pu, P = " << P << std::endl;
    }

    gridpack::math::Finalize();
    return 0;
}