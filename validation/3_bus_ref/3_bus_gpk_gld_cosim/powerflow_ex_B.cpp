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
    const double VBASE = 2401.7771;
    const double period = 900.0;
    const double stopTime = 86400.0;

    const std::complex<double> ROT_B = std::polar(1.0, -2.0 * M_PI / 3.0);

    gridpack::Environment env(argc, argv);
    gridpack::math::Initialize(&argc, &argv);

    helics::FederateInfo fi;
    fi.coreType = helics::CoreType::TCP;
    fi.coreInitString = "--broker=127.0.0.1:23500 --localport=23602 --ipv4";
    fi.setProperty(HELICS_PROPERTY_INT_LOG_LEVEL, HELICS_LOG_LEVEL_DEBUG);
    fi.setProperty(HELICS_PROPERTY_TIME_PERIOD, period);
    fi.setFlagOption(HELICS_FLAG_UNINTERRUPTIBLE, false);
    fi.setFlagOption(HELICS_FLAG_TERMINATE_ON_ERROR, true);

    helics::ValueFederate fed("gridpack_b", fi);

    auto pubVb = fed.registerPublication("Vb", "complex", "V");
    auto subSb = fed.registerSubscription("gld_hlc_conn/Sb", "VA");

    std::ofstream outFile("gpk_3bus_connB.csv");
    outFile << "t_s,timestamp,Sb_MW,Sb_Mvar,Vret_pu_re,Vret_pu_im,Vpub_pu_re,Vpub_pu_im\n";

    {
        std::ofstream clr("gpk_cosim_timeseries_phaseB.csv");
    }

    gridpack::powerflow::PFApp app;

    fed.enterExecutingMode();

    double grantedTime = 0.0;

    std::complex<double> Vb0_pu(-0.5, -0.866025403784);
    pubVb.publish(Vb0_pu * VBASE);

    char exeName[] = "powerflow_ex_B.x";
    char xmlName[] = "input_3_bus_phase_B.xml";
    char phaseTag[] = "B";
    char* pfArgv[] = { exeName, xmlName, phaseTag, nullptr };
    int pfArgc = 3;

    while (grantedTime + period <= stopTime) {
        grantedTime = fed.requestTime(grantedTime + period);

        std::complex<double> Sb_VA = subSb.getValue<std::complex<double>>();
        std::complex<double> Sb_MW = Sb_VA / 1e6;
        Sb_MW = limitMVA(Sb_MW, 100.0);

        std::complex<double> Vret_pu(1.0, 0.0);
        app.execute(pfArgc, pfArgv, gridpack::powerflow::Phase::B, Vret_pu, Sb_MW);

        std::complex<double> Vpub_pu = Vret_pu * ROT_B;
        std::string ts = simTimeToTimestamp(grantedTime);

        outFile << grantedTime << ","
                << ts << ","
                << Sb_MW.real() << ","
                << Sb_MW.imag() << ","
                << Vret_pu.real() << ","
                << Vret_pu.imag() << ","
                << Vpub_pu.real() << ","
                << Vpub_pu.imag() << "\n";

        app.appendBusVoltagesTimeSeriesCSV("gpk_cosim_timeseries_phaseB.csv", ts, "B");

        pubVb.publish(Vpub_pu * VBASE);

        std::cout << "[B t=" << grantedTime << " | " << ts << "] Sb(MW+jMvar)="
                  << Sb_MW << "  Vb_published(pu)=" << Vpub_pu << "\n";
    }

    fed.finalize();
    outFile.close();
    gridpack::math::Finalize();
    return 0;
}
