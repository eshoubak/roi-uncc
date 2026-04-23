/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */

#ifndef _pf_app_h_
#define _pf_app_h_

#include <string>
#include "boost/smart_ptr/shared_ptr.hpp"
#include "pf_factory.hpp"

namespace gridpack {
    namespace powerflow {

        class PFApp
        {
        public:
            PFApp(void);
            ~PFApp(void);

            void execute(int argc, char** argv);

            // Snapshot export
            void writeBusVoltagesCSV(const std::string& filename) const;

            // Time-series export (append rows)
            void appendBusVoltagesTimeSeriesCSV(const std::string& filename,
                const std::string& timestamp,
                const std::string& phase_label) const;

            double getBoundaryMagnitude() const;
            double getBoundaryPinjection() const;
            void setBoundaryPinjection(double P);

        private:
            double boundary_Vmag_;
            double boundary_Pinj_;
            boost::shared_ptr<PFNetwork> p_network_;
        };

    } // namespace powerflow
} // namespace gridpack

#endif // _pf_app_h_