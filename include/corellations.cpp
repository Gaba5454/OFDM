#include "corellations.h"


std::vector<double> correlationPSS(
    const std::vector<CF>& RxArray, 
    const std::vector<CF>& PSS) 
{
    const size_t len_rx = RxArray.size();
    const size_t len_pss = PSS.size();
    
    if (len_pss == 0 || len_rx < len_pss) {
        return {};
    }
    
    double pss_energy = 0.0;
    for (const auto& s : PSS) {
        pss_energy += std::norm(s);  
    }
    if (pss_energy == 0.0) {
        return {};
    }
    
    const size_t output_size = len_rx - len_pss + 1;
    std::vector<double> corrArr;
    corrArr.reserve(output_size);
    
    for (size_t k = 0; k < output_size; ++k) {
        CF sum(0.0, 0.0);
        double rx_energy = 0.0;
        
        for (size_t n = 0; n < len_pss; ++n) {
            const CF& rx = RxArray[k + n];
            sum += rx * std::conj(PSS[n]);
            rx_energy += std::norm(rx);
        }

        double correlation = 0.0;
        if (rx_energy > 0.0) {
            correlation = std::abs(sum) / std::sqrt(pss_energy * rx_energy);
        }

        corrArr.push_back(correlation);
    }
    
    return corrArr;
}


size_t findCorrelationPeak(const std::vector<double>& corr_map) {
    
    if (corr_map.empty()) {
        std::cerr << "Error: Correlation map is empty!" << std::endl;
        return SIZE_MAX;
    }
    
    auto max_it = std::max_element(corr_map.begin(), corr_map.end());
    double max_val = *max_it;
    
    if (max_val < 0.2) {
        return SIZE_MAX;  
    }
    
    return static_cast<size_t>(std::distance(corr_map.begin(), max_it));
}
