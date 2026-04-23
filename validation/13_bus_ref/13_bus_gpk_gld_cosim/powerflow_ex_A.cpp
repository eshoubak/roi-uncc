#include <iostream>
#include <fstream>
#include <complex>
#include <cmath>

#include <helics/application_api/ValueFederate.hpp>

#include "gridpack/include/gridpack.hpp"
#include "pf_app.hpp"

static std::complex<double> limitMVA(std::complex<double> S_mva, double max_mva)
{
    double mag = std::abs(S_mva);
    if (mag > max_mva && mag > 0.0) return S_mva * (max_mva / mag);
    return S_mva;
}

int main(int argc, char** argv)
{
    // ---- User constants ----
    const double VBASE = 2401.7771;     // volts
    const double period = 900.0;         // seconds (verification)
    const double stopTime = 86400.0;     // seconds (1 hour)

    // ---------- HELICS Federate ----------
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

    std::ofstream outFile("gpk_13bus_connA.csv");
    outFile << "t_s,Sa_MW,Sa_Mvar,Vret_pu_re,Vret_pu_im\n";

    // ---------- GridPACK ----------
    gridpack::Environment env(argc, argv);
    gridpack::powerflow::PFApp app;

    fed.enterExecutingMode();

    double grantedTime = 0.0;

    // Initial voltage (ABC frame)
    std::complex<double> Va0_pu(1.0, 0.0);
    pubVa.publish(Va0_pu * VBASE);

    // Clean argv for PFApp
    char exeName[] = "powerflow_ex_A.x";
    char xmlName[] = "input_13_bus_phase_A.xml";
    char phaseTag[] = "A";
    char* pfArgv[] = { exeName, xmlName, phaseTag, nullptr };
    int pfArgc = 3;

    while (grantedTime + period <= stopTime) {
        grantedTime = fed.requestTime(grantedTime + period);

        // GLD publishes complex power in VA
        std::complex<double> Sa_VA = subSa.getValue<std::complex<double>>();
        std::complex<double> Sa_MW = Sa_VA / 1e6; // VA -> (MW+jMvar) numerically
        Sa_MW = limitMVA(Sa_MW, 100.0);

        std::complex<double> Vret_pu(1.0, 0.0);
        app.execute(pfArgc, pfArgv, gridpack::powerflow::Phase::A, Vret_pu, Sa_MW);

        outFile << grantedTime << "," << Sa_MW.real() << "," << Sa_MW.imag()
            << "," << Vret_pu.real() << "," << Vret_pu.imag() << "\n";

        // A has no rotation
        pubVa.publish(Vret_pu * VBASE);

        std::cout << "[A t=" << grantedTime << "] Sa(MW+jMvar)=" << Sa_MW
            << "  Va(pu)=" << Vret_pu << "\n";
    }

    fed.finalize();
    outFile.close();
    return 0;
}