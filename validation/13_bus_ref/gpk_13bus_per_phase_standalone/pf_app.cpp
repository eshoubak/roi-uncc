/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */

#include "gridpack/include/gridpack.hpp"
#include "pf_app.hpp"
#include "pf_factory.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdio>

namespace {
    const int BOUNDARY_BUS_NUMBER = 3;
}

namespace gridpack {
    namespace powerflow {

        PFApp::PFApp(void)
            : boundary_Vmag_(0.0)
            , boundary_Pinj_(0.0)
            , p_network_()
        {
        }

        PFApp::~PFApp(void) {}

        enum Parser { PTI23, PTI33 };

        void PFApp::writeBusVoltagesCSV(const std::string& filename) const
        {
            if (!p_network_) {
                std::cerr << "[PFApp] ERROR: p_network_ is null. Call execute() first.\n";
                return;
            }

            gridpack::parallel::Communicator world = p_network_->communicator();

            std::string outname = filename;
            if (world.size() > 1) {
                outname = filename + ".rank" + std::to_string(world.rank());
            }

            std::ofstream out(outname.c_str());
            if (!out.is_open()) {
                std::cerr << "[PFApp] ERROR: cannot open " << outname << " for writing\n";
                return;
            }

            out << "Original Bus Number,Voltage Magnitude (pu),Voltage Angle (deg)\n";
            out << std::fixed << std::setprecision(10);

            int nbus = p_network_->numBuses();
            for (int i = 0; i < nbus; ++i) {
                int busnum = -1;
                gridpack::component::DataCollection* data = p_network_->getBusData(i).get();
                if (data) data->getValue(BUS_NUMBER, &busnum);
                if (busnum < 0) busnum = i + 1;

                ComplexType V = p_network_->getBus(i)->getComplexVoltage();
                double vmag = std::abs(V);
                double vang_deg = std::atan2(imag(V), real(V)) * 180.0 / M_PI;

                out << busnum << "," << vmag << "," << vang_deg << "\n";
            }

            out.close();

            if (world.rank() == 0) {
                std::cout << "[PFApp] Wrote bus voltages to " << outname << "\n";
                if (world.size() > 1) {
                    std::cout << "[PFApp] NOTE: MPI size > 1 => per-rank files written.\n";
                }
            }
        }

        void PFApp::appendBusVoltagesTimeSeriesCSV(const std::string& filename,
            const std::string& timestamp,
            const std::string& phase_label) const
        {
            if (!p_network_) {
                std::cerr << "[PFApp] ERROR: p_network_ is null. Call execute() first.\n";
                return;
            }

            gridpack::parallel::Communicator world = p_network_->communicator();
            if (world.rank() != 0) return;

            bool file_exists = false;
            {
                std::ifstream fin(filename.c_str());
                file_exists = fin.good();
            }

            std::ofstream out(filename.c_str(), std::ios::app);
            if (!out.is_open()) {
                std::cerr << "[PFApp] ERROR: cannot open " << filename << " for appending\n";
                return;
            }

            if (!file_exists) {
                out << "timestamp,bus,phase,vmag_pu,vang_deg\n";
            }

            out << std::fixed << std::setprecision(10);

            int nbus = p_network_->numBuses();
            for (int i = 0; i < nbus; ++i) {
                int busnum = -1;
                gridpack::component::DataCollection* data = p_network_->getBusData(i).get();
                if (data) data->getValue(BUS_NUMBER, &busnum);
                if (busnum < 0) busnum = i + 1;

                ComplexType V = p_network_->getBus(i)->getComplexVoltage();
                double vmag = std::abs(V);
                double vang_deg = std::atan2(imag(V), real(V)) * 180.0 / M_PI;

                out << timestamp << ","
                    << busnum << ","
                    << phase_label << ","
                    << vmag << ","
                    << vang_deg << "\n";
            }

            out.close();
        }

        void PFApp::execute(int argc, char** argv)
        {
            gridpack::parallel::Communicator world;

            boost::shared_ptr<PFNetwork> network(new PFNetwork(world));
            p_network_ = network;

            gridpack::utility::Configuration* config =
                gridpack::utility::Configuration::configuration();
            config->enableLogging(&std::cout);

            bool opened;
            if (argc >= 2 && argv[1] != NULL) {
                char inputfile[256];
                std::sprintf(inputfile, "%s", argv[1]);
                opened = config->open(inputfile, world);
            }
            else {
                opened = config->open("input.xml", world);
            }
            if (!opened) return;

            gridpack::utility::Configuration::CursorPtr cursor;
            cursor = config->getCursor("Configuration.Powerflow");

            std::string filename;
            int filetype = PTI23;

            if (!cursor->get("networkConfiguration", &filename)) {
                if (cursor->get("networkConfiguration_v33", &filename)) {
                    filetype = PTI33;
                }
                else {
                    std::printf("No network configuration file specified\n");
                    return;
                }
            }

            double tolerance = cursor->get("tolerance", 1.0e-6);
            int max_iteration = cursor->get("maxIteration", 50);
            ComplexType tol;
            double phaseShiftSign = cursor->get("phaseShiftSign", 1.0);

            if (world.rank() == 0) std::printf("Network filename: (%s)\n", filename.c_str());

            if (filetype == PTI23) {
                if (world.rank() == 0) std::printf("Using V23 parser\n");
                gridpack::parser::PTI23_parser<PFNetwork> parser(network);
                parser.parse(filename.c_str());
                if (phaseShiftSign == -1.0) parser.changePhaseShiftSign();
            }
            else if (filetype == PTI33) {
                if (world.rank() == 0) std::printf("Using V33 parser\n");
                gridpack::parser::PTI33_parser<PFNetwork> parser(network);
                parser.parse(filename.c_str());
                if (phaseShiftSign == -1.0) parser.changePhaseShiftSign();
            }

            network->partition();

            std::printf("Process: %d NBUS: %d NBRANCH: %d\n",
                world.rank(), network->numBuses(), network->numBranches());

            gridpack::serial_io::SerialBusIO<PFNetwork> busIO(8192, network);
            char ioBuf[128];
            std::sprintf(ioBuf, "\nMaximum number of iterations: %d\n", max_iteration);
            busIO.header(ioBuf);
            std::sprintf(ioBuf, "\nConvergence tolerance: %f\n", tolerance);
            busIO.header(ioBuf);

            gridpack::powerflow::PFFactory factory(network);
            factory.load();
            factory.setComponents();
            factory.setExchange();
            network->initBusUpdate();
            factory.setYBus();
            factory.setSBus();
            busIO.header("\nIteration 0\n");

            factory.setMode(RHS);
            gridpack::mapper::BusVectorMap<PFNetwork> vMap(network);
            boost::shared_ptr<gridpack::math::Vector> PQ = vMap.mapToVector();

            factory.setMode(Jacobian);
            gridpack::mapper::FullMatrixMap<PFNetwork> jMap(network);
            boost::shared_ptr<gridpack::math::Matrix> J = jMap.mapToMatrix();

            boost::shared_ptr<gridpack::math::Vector> X(PQ->clone());

            gridpack::math::LinearSolver solver(*J);
            solver.configure(cursor);

            tol = 2.0 * tolerance;
            int iter = 0;

            X->zero();
            busIO.header("\nCalling solver\n");
            solver.solve(*PQ, *X);
            tol = PQ->normInfinity();

            while (real(tol) > tolerance && iter < max_iteration) {
                factory.setMode(RHS);
                vMap.mapToBus(X);
                network->updateBuses();

                vMap.mapToVector(PQ);
                factory.setMode(Jacobian);
                jMap.mapToMatrix(J);

                X->zero();
                solver.solve(*PQ, *X);

                tol = PQ->normInfinity();
                std::sprintf(ioBuf, "\nIteration %d Tol: %12.6e\n", iter + 1, real(tol));
                busIO.header(ioBuf);
                iter++;
            }

            factory.setMode(RHS);
            vMap.mapToBus(X);
            network->updateBuses();

            gridpack::serial_io::SerialBranchIO<PFNetwork> branchIO(512, network);
            branchIO.header("\n   Branch Power Flow\n");
            branchIO.header("\n        Bus 1       Bus 2   CKT         P                    Q\n");
            branchIO.write();

            busIO.header("\n   Bus Voltages and Phase Angles\n");
            busIO.header("\n   Bus Number      Phase Angle      Voltage Magnitude\n");
            busIO.write();

            writeBusVoltagesCSV("bus_voltages.csv");

            int boundary_bus_num = BOUNDARY_BUS_NUMBER;
            int boundary_bus_index = boundary_bus_num - 1;

            if (boundary_bus_index >= 0 && boundary_bus_index < network->numBuses()) {
                boundary_Vmag_ = network->getBus(boundary_bus_index)->getVoltage();

                double pll = 0.0;
                bool ok = false;
                try {
                    ok = network->getBusData(boundary_bus_index)->getValue(LOAD_PL, &pll, 0);
                }
                catch (...) {
                    ok = false;
                }
                boundary_Pinj_ = (ok ? pll : 0.0);

                if (world.rank() == 0) {
                    std::cout << "[PFApp] Boundary bus " << boundary_bus_num
                        << " Vmag=" << boundary_Vmag_
                        << " PL=" << boundary_Pinj_
                        << " (LOAD_PL at boundary bus)\n";
                }
            }
            else {
                boundary_Vmag_ = 0.0;
                boundary_Pinj_ = 0.0;
                if (world.rank() == 0) {
                    std::cout << "[PFApp] WARNING: boundary_bus_index out of range\n";
                }
            }
        }

        double PFApp::getBoundaryMagnitude() const { return boundary_Vmag_; }
        double PFApp::getBoundaryPinjection() const { return boundary_Pinj_; }
        void PFApp::setBoundaryPinjection(double P) { boundary_Pinj_ = P; }

    } // namespace powerflow
} // namespace gridpack