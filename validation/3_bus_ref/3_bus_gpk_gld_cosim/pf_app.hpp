#ifndef _pf_app_h_
#define _pf_app_h_

#include "boost/smart_ptr/shared_ptr.hpp"
#include "pf_factory.hpp"
#include <complex>
#include <string>

namespace gridpack {
namespace powerflow {

// Phase enum used by the per-phase overload
enum class Phase { A, B, C };

class PFApp
{
public:
  PFApp(void);
  ~PFApp(void);

  // Single-phase solve
  void execute(int argc,
               char** argv,
               Phase ph,
               std::complex<double>& V,
               std::complex<double>& S);

  // Full 3-phase interface
  void execute(int argc,
               char** argv,
               std::complex<double>& Va,
               std::complex<double>& Vb,
               std::complex<double>& Vc,
               std::complex<double>& Sa,
               std::complex<double>& Sb,
               std::complex<double>& Sc);

  // Append all bus voltages for one timestamp
  void appendBusVoltagesTimeSeriesCSV(const std::string& filename,
                                      const std::string& timestamp,
                                      const std::string& phase_label) const;

private:
  void runOnePhase_(int argc,
                    char** argv,
                    Phase ph,
                    const std::complex<double>& Sinj,
                    std::complex<double>& Vboundary);

  // store solved network from most recent run
  boost::shared_ptr<PFNetwork> p_network_;
  Phase last_phase_;
};

} // namespace powerflow
} // namespace gridpack

#endif
