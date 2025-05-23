// report_generator.hpp
#pragma once
#include <fstream>
#include <string>
#include "apb_analyzer.hpp"  // For getting PADDR/PWDATA fault info
#include "error_logger.hpp"
#include "statistics.hpp"

namespace APBSystem {

class ReportGenerator {
   public:
    void generate_report(const std::string& output_filename,
                         const Statistics& stats,
                         const ErrorLogger& error_logger,  // Already sorted
                         const ApbAnalyzer& analyzer,
                         double cpu_elapsed_time_ms);

   private:
    void write_completer_connection_report(std::ofstream& outfile,
                                           int completer_idx_0_based,  // 0 to MAX_COMPLETERS-1
                                           const IdentifiedFloatingPair& paddr_fault,
                                           int paddr_bus_width,
                                           const IdentifiedFloatingPair& pwdata_fault,
                                           int pwdata_bus_width);
};

}  // namespace APBSystem