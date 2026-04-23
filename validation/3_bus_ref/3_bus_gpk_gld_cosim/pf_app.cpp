#include "gridpack/include/gridpack.hpp"
#include "pf_app.hpp"
#include "pf_factory.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <complex>
#include <cmath>
#include <iomanip>

gridpack::powerflow::PFApp::PFApp(void)
  : p_network_(), last_phase_(gridpack::powerflow::Phase::A)
{
}

gridpack::powerflow::PFApp::~PFApp(void) {}

namespace {
    enum Parser { PTI23, PTI33 };

    static std::string phaseToString(gridpack::powerflow::Phase ph)
    {
        switch (ph) {
        case gridpack::powerflow::Phase::A: return "A";
        case gridpack::powerflow::Phase::B: return "B";
        case gridpack::powerflow::Phase::C: return "C";
        default: return "A";
        }
    }

    static std::string defaultXmlForPhase(gridpack::powerflow::Phase ph)
    {
        switch (ph) {
        case gridpack::powerflow::Phase::A: return "input_3_bus_phase_A.xml";
        case gridpack::powerflow::Phase::B: return "input_3_bus_phase_B.xml";
        case gridpack::powerflow::Phase::C: return "input_3_bus_phase_C.xml";
        default: return "input_3_bus_phase_A.xml";
        }
    }

    static int findLocalBusIndexByOriginal(
        const boost::shared_ptr<gridpack::powerflow::PFNetwork>& network,
        int orig_bus)
    {
        for (int i = 0; i < network->numBuses(); ++i) {
            if (network->getOriginalBusIndex(i) == orig_bus) return i;
        }
        return -1;
    }

    // A: 0 deg, B: -120 deg, C: +120 deg
    static double phaseRotationRad(gridpack::powerflow::Phase ph)
    {
        const double deg2rad = M_PI / 180.0;
        switch (ph) {
        case gridpack::powerflow::Phase::A: return 0.0;
        case gridpack::powerflow::Phase::B: return -120.0 * deg2rad;
        case gridpack::powerflow::Phase::C: return +120.0 * deg2rad;
        default: return 0.0;
        }
    }

    static std::complex<double> rotatePhasor(const std::complex<double>& z, double ang_rad)
    {
        return z * std::polar(1.0, ang_rad);
    }
} // end anon namespace


void gridpack::powerflow::PFApp::execute(int argc,
    char** argv,
    Phase ph,
    std::complex<double>& V,
    std::complex<double>& S)
{
    runOnePhase_(argc, argv, ph, S, V);
}

void gridpack::powerflow::PFApp::execute(int argc,
    char** argv,
    std::complex<double>& Va,
    std::complex<double>& Vb,
    std::complex<double>& Vc,
    std::complex<double>& Sa,
    std::complex<double>& Sb,
    std::complex<double>& Sc)
{
    runOnePhase_(argc, argv, Phase::A, Sa, Va);
    runOnePhase_(argc, argv, Phase::B, Sb, Vb);
    runOnePhase_(argc, argv, Phase::C, Sc, Vc);
}

void gridpack::powerflow::PFApp::appendBusVoltagesTimeSeriesCSV(
    const std::string& filename,
    const std::string& timestamp,
    const std::string& phase_label) const
{
    if (!p_network_) {
        std::cerr << "[PFApp] ERROR: p_network_ is null. Solve first.\n";
        return;
    }

    gridpack::parallel::Communicator world = p_network_->communicator();
    if (world.rank() != 0) return;

    std::ofstream out(filename.c_str(), std::ios::app);
    if (!out.is_open()) {
        std::cerr << "[PFApp] ERROR: cannot open " << filename << " for appending\n";
        return;
    }

    out << std::fixed << std::setprecision(10);

    const double rot = phaseRotationRad(last_phase_);

    for (int i = 0; i < p_network_->numBuses(); ++i) {
        int busnum = p_network_->getOriginalBusIndex(i);
        double vmag_i = p_network_->getBus(i)->getVoltage();
        double vang_i = p_network_->getBus(i)->getPhase();

        out << timestamp << ","
            << busnum << ","
            << phase_label << ","
            << vmag_i << ","
            << (vang_i + rot) * 180.0 / M_PI
            << "\n";
    }

    out.close();
}

void gridpack::powerflow::PFApp::runOnePhase_(int argc,
    char** argv,
    Phase ph,
    const std::complex<double>& Sinj,
    std::complex<double>& Vboundary)
{
    gridpack::parallel::Communicator world;
    boost::shared_ptr<PFNetwork> network(new PFNetwork(world));
    p_network_ = network;
    last_phase_ = ph;

    gridpack::utility::Configuration* config =
        gridpack::utility::Configuration::configuration();
    config->enableLogging(&std::cout);

    std::string xmlfile;
    if (argc >= 2 && argv[1] != NULL) {
        xmlfile = argv[1];
    } else {
        xmlfile = defaultXmlForPhase(ph);
    }

    bool opened = config->open(xmlfile.c_str(), world);
    if (!opened) {
        if (world.rank() == 0) {
            std::cerr << "ERROR: Could not open XML config: " << xmlfile << "\n";
        }
        return;
    }

    gridpack::utility::Configuration::CursorPtr cursor =
        config->getCursor("Configuration.Powerflow");

    std::string filename;
    int filetype = PTI23;
    if (!cursor->get("networkConfiguration", &filename)) {
        if (cursor->get("networkConfiguration_v33", &filename)) {
            filetype = PTI33;
        } else {
            if (world.rank() == 0) {
                std::cerr << "ERROR: No network configuration file specified\n";
            }
            return;
        }
    }

    int boundary_orig_bus = cursor->get("boundaryBus", 3);

    double tolerance = cursor->get("tolerance", 1.0e-6);
    int max_iteration = cursor->get("maxIteration", 50);
    double phaseShiftSign = cursor->get("phaseShiftSign", 1.0);

    if (world.rank() == 0) {
        std::cout << "=== PF solve phase " << phaseToString(ph) << " ===\n";
        std::cout << "XML: " << xmlfile << "\n";
        std::cout << "Network filename: (" << filename << ")\n";
        std::cout << "Boundary original bus: " << boundary_orig_bus << "\n";
    }

    if (filetype == PTI23) {
        if (world.rank() == 0) std::cout << "Using V23 parser\n";
        gridpack::parser::PTI23_parser<PFNetwork> parser(network);
        parser.parse(filename.c_str());
        if (phaseShiftSign == -1.0) parser.changePhaseShiftSign();
    } else {
        if (world.rank() == 0) std::cout << "Using V33 parser\n";
        gridpack::parser::PTI33_parser<PFNetwork> parser(network);
        parser.parse(filename.c_str());
        if (phaseShiftSign == -1.0) parser.changePhaseShiftSign();
    }

    int matched_index = findLocalBusIndexByOriginal(network, boundary_orig_bus);
    if (matched_index < 0) {
        if (world.rank() == 0) {
            std::cerr << "ERROR: Original bus number " << boundary_orig_bus
                      << " not found in network.\n";
        }
        return;
    }

    // apply injected complex power at boundary bus
    network->getBusData(matched_index)->setValue(LOAD_PL, Sinj.real(), 0);
    network->getBusData(matched_index)->setValue(LOAD_QL, Sinj.imag(), 0);

    network->partition();

    gridpack::powerflow::PFFactory factory(network);
    factory.load();
    factory.setComponents();
    factory.setExchange();
    network->initBusUpdate();
    factory.setYBus();
    factory.setSBus();

    factory.setMode(RHS);
    gridpack::mapper::BusVectorMap<PFNetwork> vMap(network);
    boost::shared_ptr<gridpack::math::Vector> PQ = vMap.mapToVector();

    factory.setMode(Jacobian);
    gridpack::mapper::FullMatrixMap<PFNetwork> jMap(network);
    boost::shared_ptr<gridpack::math::Matrix> J = jMap.mapToMatrix();

    boost::shared_ptr<gridpack::math::Vector> X(PQ->clone());
    gridpack::math::LinearSolver solver(*J);
    solver.configure(cursor);

    std::complex<double> tol = 2.0 * tolerance;
    int iter = 0;
    X->zero();

    solver.solve(*PQ, *X);
    tol = PQ->normInfinity();

    while (std::real(tol) > tolerance && iter < max_iteration) {
        factory.setMode(RHS);
        vMap.mapToBus(X);
        network->updateBuses();
        vMap.mapToVector(PQ);

        factory.setMode(Jacobian);
        jMap.mapToMatrix(J);

        X->zero();
        solver.solve(*PQ, *X);
        tol = PQ->normInfinity();
        iter++;
    }

    factory.setMode(RHS);
    vMap.mapToBus(X);
    network->updateBuses();

    if (world.rank() == 0) {
        std::cout << "Iterations: " << iter
                  << "  final ||F||inf=" << std::real(tol) << "\n";
    }

    // return boundary voltage phasor rotated into physical phase reference
    double vmag = network->getBus(matched_index)->getVoltage();
    double vang = network->getBus(matched_index)->getPhase();
    std::complex<double> Vsolver = std::polar(vmag, vang);
    Vboundary = rotatePhasor(Vsolver, phaseRotationRad(ph));

    // keep your snapshot bus-voltage file too
    std::string phase_letter = phaseToString(ph);
    if (argc >= 3 && argv[2] != NULL) {
        phase_letter = argv[2];
    }
    std::string filename_out = "bus_voltages_phase" + phase_letter + ".csv";

    if (world.rank() == 0) {
        std::ofstream outFile(filename_out.c_str());
        outFile << "Original Bus Number,Voltage Magnitude (pu),Voltage Angle (rad)\n";

        const double rot = phaseRotationRad(ph);

        for (int i = 0; i < network->numBuses(); ++i) {
            int busnum = network->getOriginalBusIndex(i);
            double vmag_i = network->getBus(i)->getVoltage();
            double vang_i = network->getBus(i)->getPhase();
            outFile << busnum << "," << vmag_i << "," << (vang_i + rot) << "\n";
        }
        outFile.close();
        std::cout << "Bus voltages written to " << filename_out << "\n";
    }
}
