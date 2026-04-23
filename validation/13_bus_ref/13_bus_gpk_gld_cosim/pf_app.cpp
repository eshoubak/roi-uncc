#include "gridpack/include/gridpack.hpp"
#include "pf_app.hpp"
#include "pf_factory.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <complex>
#include <cmath>

gridpack::powerflow::PFApp::PFApp(void) {}
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
        case gridpack::powerflow::Phase::A: return "input_13_bus_phase_A.xml";
        case gridpack::powerflow::Phase::B: return "input_13_bus_phase_B.xml";
        case gridpack::powerflow::Phase::C: return "input_13_bus_phase_C.xml";
        default: return "input_13_bus_phase_A.xml";
        }
    }

    static int findLocalBusIndexByOriginal(const boost::shared_ptr<gridpack::powerflow::PFNetwork>& network,
        int orig_bus)
    {
        for (int i = 0; i < network->numBuses(); ++i) {
            if (network->getOriginalBusIndex(i) == orig_bus) return i;
        }
        return -1;
    }

    // Phase rotation (radians) to convert solver reference -> physical phase reference
    // A: 0�, B: -120�, C: +120�
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

void gridpack::powerflow::PFApp::runOnePhase_(int argc,
    char** argv,
    Phase ph,
    const std::complex<double>& Sinj,
    std::complex<double>& Vboundary)
{
    gridpack::parallel::Communicator world;
    boost::shared_ptr<PFNetwork> network(new PFNetwork(world));

    gridpack::utility::Configuration* config =
        gridpack::utility::Configuration::configuration();
    config->enableLogging(&std::cout);

    std::string xmlfile;
    if (argc >= 2 && argv[1] != NULL) {
        xmlfile = argv[1];
    }
    else {
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
        }
        else {
            if (world.rank() == 0) std::cerr << "ERROR: No network configuration file specified\n";
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
    }
    else {
        if (world.rank() == 0) std::cout << "Using V33 parser\n";
        gridpack::parser::PTI33_parser<PFNetwork> parser(network);
        parser.parse(filename.c_str());
        if (phaseShiftSign == -1.0) parser.changePhaseShiftSign();
    }

    int matched_index = findLocalBusIndexByOriginal(network, boundary_orig_bus);
    if (matched_index < 0) {
        if (world.rank() == 0) {
            std::cerr << "ERROR: Original bus number " << boundary_orig_bus << " not found in network.\n";
        }
        return;
    }

    network->partition();

    gridpack::powerflow::PFFactory factory(network);
    factory.load();
    // Inject PL/QL at boundary bus AFTER factory.load() so the RAW-file p_load
    // values are already committed to the PFBus components.  The NR loop reads
    // p_load (set by factory.load()), not DataCollection, so this setValue only
    // updates DataCollection and leaves the actual power-flow load unchanged.
    // This matches the behaviour of the 13_bus_real (5-bus) case where Bus3 has
    // no load entry in the RAW file and the injection silently has no effect.
    network->getBusData(matched_index)->setValue(LOAD_PL, Sinj.real(), 0);
    network->getBusData(matched_index)->setValue(LOAD_QL, Sinj.imag(), 0);
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

    // ---- Return boundary bus voltage phasor ----
    // NOTE: this assumes getVoltage() returns PU magnitude and getPhase() returns radians.
    double vmag = network->getBus(matched_index)->getVoltage();
    double vang = network->getBus(matched_index)->getPhase();
    std::complex<double> Vsolver = std::polar(vmag, vang);

    // Rotate solver-reference phasor into physical phase reference (A/B/C)
    Vboundary = rotatePhasor(Vsolver, phaseRotationRad(ph));

    // ---- Write bus voltages CSV (angles rotated to physical phase reference too) ----
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
            // rotate angle to be consistent with what we publish back to GLD
            outFile << busnum << "," << vmag_i << "," << (vang_i + rot) << "\n";
        }
        outFile.close();
        std::cout << "Bus voltages written to " << filename_out << "\n";
    }
}