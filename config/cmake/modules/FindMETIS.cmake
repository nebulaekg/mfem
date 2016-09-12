# Defines the following variables:
#   - METIS_FOUND
#   - METIS_LIBRARIES
#   - METIS_INCLUDE_DIRS

include(MfemCmakeUtilities)
mfem_find_package(METIS METIS METIS_DIR "include;Lib" "metis.h" "lib" "metis"
  "Paths to headers required by METIS." "Libraries required by METIS."
  CHECK_BUILD METIS_VERSION_5 FALSE
  "
#include <metis.h>
#include <cstddef> // So NULL is defined

int main()
{
    int n = 10;
    int nparts = 5;
    int edgecut;
    int* partitioning = new int[10];
    int* I = partitioning,
       * J = partitioning;

    int ncon = 1;
    int err;
    int options[40];

    METIS_SetDefaultOptions(options);
    options[10] = 1; // set METIS_OPTION_CONTIG

    err = METIS_PartGraphKway(&n,
                              &ncon,
                              I,
                              J,
                              (idx_t *) NULL,
                              (idx_t *) NULL,
                              (idx_t *) NULL,
                              &nparts,
                              (real_t *) NULL,
                              (real_t *) NULL,
                              options,
                              &edgecut,
                              partitioning);
    return err;
}
")

# Expose METIS_VERSION_5 (it is created as INTERNAL) and copy its value to
# MFEM_USE_METIS_5:
set(MFEM_USE_METIS_5 ${METIS_VERSION_5})
unset(METIS_VERSION_5 CACHE)
set(METIS_VERSION_5 ${MFEM_USE_METIS_5} CACHE BOOL "Is METIS version 5?")
