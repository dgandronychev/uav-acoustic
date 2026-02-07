#include "core/tflite/tcn_detector.h"

#include <fstream>
#include <iostream>
#include <algorithm>

namespace core::ml {

    std::vector<std::string> TcnDetector::LoadLines(const std::string& path) {
        std::vector<std::string> lines;
        std::ifstream in(path);
        if (!in) return lines;
        std::string s;
        while (std::getline(in, s)) {
            if (!s.empty() && s.back() == '\r') s.pop_back();
            if (!s.empty()) lines.push_back(s);
        }
        return lines;
    }

    TcnDetector::TcnDetector(const Config& cfg)
        : cfg_(cfg),
        runner_(cfg.model_path),
        class_names_(LoadLines(cfg.class_names_path)) {
        if (!runner_.IsValid()) {
            std::cerr << "[TcnDetector] runner invalid\n";
            return;
        }
        if (class_names_.empty()) {
            std::cerr << "[TcnDetector] class_names empty: " << cfg_.class_names_path << "\n";
            return;
        }

        // Basic sanity check: expected input size 1*128*169*1 = 21632
        const int expected = cfg_.n_mels * cfg_.n_frames;
        if (runner_.input_size() != expected) {
            std::cerr << "[TcnDetector] input_size mismatch. runner=" << runner_.input_size()
                << " expected=" << expected << "\n";
        }
    }

    int TcnDetector::Run(const float* pcen_window, int pcen_size, std::vector<float>* out_scores) {
        if (!IsValid() || !pcen_window || pcen_size <= 0) return -1;

        const int expected = cfg_.n_mels * cfg_.n_frames;
        if (pcen_size != expected) return -1;

        auto scores = runner_.RunFloat(pcen_window, pcen_size);
        if (scores.empty()) return -1;

        if (out_scores) *out_scores = scores;

        // argmax
        int best = 0;
        float bestv = scores[0];
        for (int i = 1; i < static_cast<int>(scores.size()); ++i) {
            if (scores[i] > bestv) {
                bestv = scores[i];
                best = i;
            }
        }
        return best;
    }

}  // namespace core::ml
