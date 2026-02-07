#pragma once

#include <string>
#include <vector>

#include "core/tflite/tflite_runner.h"

namespace core::ml {

    // Minimal detector wrapper for your model input shape (1, 128, 169, 1).
    // Feeds float32 input for dynamic-quant tflite.
    class TcnDetector {
    public:
        struct Config {
            std::string model_path;      // e.g. "model_dynamic.tflite"
            std::string class_names_path; // e.g. "class_names.txt"
            int n_mels = 128;
            int n_frames = 169;          // time frames
        };

        explicit TcnDetector(const Config& cfg);

        bool IsValid() const { return runner_.IsValid() && !class_names_.empty(); }

        // Input: PCEN window flattened as float32, length = n_mels*n_frames
        // Output: fills probs/logits; returns argmax class index (or -1 on error)
        int Run(const float* pcen_window, int pcen_size, std::vector<float>* out_scores);

        const std::vector<std::string>& class_names() const { return class_names_; }

    private:
        static std::vector<std::string> LoadLines(const std::string& path);

        Config cfg_;
        TfliteRunner runner_;
        std::vector<std::string> class_names_;
    };

}  // namespace core::ml
