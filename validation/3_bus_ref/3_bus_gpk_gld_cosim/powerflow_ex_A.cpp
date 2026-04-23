#include <iostream>
#include <fstream>
#include <complex>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

#include <helics/application_api/ValueFederate.hpp>

#include "gridpack/include/gridpack.hpp"
#include "pf_app.hpp"

static std::complex<double> limitMVA(std::complex<double> S_mva, double max_mva)
{
    double mag = std::abs(S_mva);
    if (mag > max_mva && mag > 0.0) return S_mva * (max_mva / mag);
    return S_mva;
}

static std::string simTimeToTimestamp(double tsec)
{
    int total = static_cast<int>(std::llround(tsec));
    int day_offset = total / 86400;
    int rem = total % 86400;

    int hh = rem / 3600;
    int mm = (rem % 3600) / 60;
    int ss = rem % 60;

    std::ostringstream oss;
    if (day_offset == 0) {
        oss << "2000-09-01 ";
    } else {
        oss << "2000-09-02 ";
    }
    oss << std::setw(2) << std::setfill('0') << hh << ":"
        << std::setw(2) << std::setfill('0') << mm << ":"
        << std::setw(2) << std::setfill('0') << ss;
    return oss.str();
}

int main(int argc, char** argv)
{
    const double VBASE = 2401.7771;   // volts
    const double period = 900.0;      // 15 minutes
    const double stopTime = 86400.0;  // 24 hours

    gridpack::Environment env(argc, argv);
    gridpack::math::Initialize(&argc, &argv);

    helics::FederateInfo fi;
    fi.coreType = helics::CoreType::TCP;
    fi.coreInitString = "--broker=127.0.0.1:23500 --localport=23601 --ipv4";
    fi.setProperty(HELICS_PROPERTY_INT_LOG_LEVEL, HELICS_LOG_LEVEL_DEBUG);
    fi.setProperty(HELICS_PROPERTY_TIME_PERIOD, period);
    fi.setFlagOption(HELICS_FLAG_UNINTERRUPTIBLE, false);
    fi.setFlagOption(HELICS_FLAG_TERMINATE_ON_ERROR, true);

    helics::ValueFederate fed("gridpack_a", fi);

    auto pubVa = fed.registerPublication("Va", "complex", "V");
    auto subSa = fed.registerSubscription("gld_hlc_conn/Sa", "VA");

    std::ofstream outFile("gpk_3bus_connA.csv");
    outFile << "t_s,timestamp,Sa_MW,Sa_Mvar,Vret_pu_re,Vret_pu_im\n";

    // Clear the full-bus cosim time-series file first
    {
        std::ofstream clr("gpk_cosim_timeseries_phaseA.csv");
    }

    gridpack::powerflow::PFApp app;

    fed.enterExecutingMode();

    double grantedTime = 0.0;

    std::complex<double> Va0_pu(1.0, 0.0);
    pubVa.publish(Va0_pu * VBASE);

    char exeName[] = "powerflow_ex_A.x";
    char xmlName[] = "input_3_bus_phase_A.xml";
    char phaseTag[] = "A";
    char* pfArgv[] = { exeName, xmlName, phaseTag, nullptr };
    int pfArgc = 3;

    while (grantedTime + period <= stopTime) {
        grantedTime = fed.requestTime(grantedTime + period);

        std::complex<double> Sa_VA = subSa.getValue<std::complex<double>>();
        std::complex<double> Sa_MW = Sa_VA / 1e6;
        Sa_MW = limitMVA(Sa_MW, 100.0);

        std::complex<double> Vret_pu(1.0, 0.0);
        app.execute(pfArgc, pfArgv, gridpack::powerflow::Phase::A, Vret_pu, Sa_MW);

        std::string ts = simTimeToTimestamp(grantedTime);

        outFile << grantedTime << ","
                << ts << ","
                << Sa_MW.real() << ","
                << Sa_MW.imag() << ","
                << Vret_pu.real() << ","
                << Vret_pu.imag() << "\n";

        // Full-bus cosim GPK time-series
        app.appendBusVoltagesTimeSeriesCSV("gpk_cosim_timeseries_phaseA.csv", ts, "A");

        pubVa.publish(Vret_pu * VBASE);

        std::cout << "[A t=" << grantedTime << " | " << ts << "] Sa(MW+jMvar)="
                  << Sa_MW << "  Va(pu)=" << Vret_pu << "\n";
    }

    fed.finalize();
    outFile.close();
    gridpack::math::Finalize();
    return 0;
}
