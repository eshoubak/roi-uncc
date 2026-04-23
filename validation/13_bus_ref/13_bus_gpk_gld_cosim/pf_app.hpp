#ifndef _pf_app_h_
#define _pf_app_h_

#include "boost/smart_ptr/shared_ptr.hpp"
#include "pf_factory.hpp"
#include <complex>

namespace gridpack {
namespace powerflow {

// Phase enum used by the per-phase overload
enum class Phase { A, B, C };

class PFApp
{
public:
  PFApp(void);
  ~PFApp(void);

  // Option 2: single-phase solve (one exe per phase)
  void execute(int argc,
               char** argv,
               Phase ph,
               std::complex<double>& V,
               std::complex<double>& S);

  // Full 3-phase interface (kept for compatibility)
  void execute(int argc,
               char** argv,
               std::complex<double>& Va,
               std::complex<double>& Vb,
               std::complex<double>& Vc,
               std::complex<double>& Sa,
               std::complex<double>& Sb,
               std::complex<double>& Sc);

private:
  void runOnePhase_(int argc,
                    char** argv,
                    Phase ph,
                    const std::complex<double>& Sinj,
                    std::complex<double>& Vboundary);
};

} // namespace powerflow
} // namespace gridpack

#endif
