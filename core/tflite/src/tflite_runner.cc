#include "core/tflite/tflite_runner.h"

#include <cstring>
#include <iostream>

namespace core::ml {

    TfliteRunner::TfliteRunner(const std::string& model_path) {
        model_ = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
        if (!model_) {
            std::cerr << "[TfliteRunner] Failed to load model: " << model_path << "\n";
            return;
        }

        tflite::ops::builtin::BuiltinOpResolver resolver;
        tflite::InterpreterBuilder builder(*model_, resolver);
        builder(&interpreter_);
        if (!interpreter_) {
            std::cerr << "[TfliteRunner] Failed to create interpreter\n";
            return;
        }

        if (interpreter_->AllocateTensors() != kTfLiteOk) {
            std::cerr << "[TfliteRunner] AllocateTensors() failed\n";
            return;
        }

        // Input
        const int in_idx = interpreter_->inputs().empty() ? -1 : interpreter_->inputs()[0];
        if (in_idx < 0) {
            std::cerr << "[TfliteRunner] No input tensor\n";
            return;
        }
        TfLiteTensor* in = interpreter_->tensor(in_idx);
        if (!in) return;

        // Expect float32 for dynamic quant model
        if (in->type != kTfLiteFloat32) {
            std::cerr << "[TfliteRunner] Unexpected input type: " << in->type
                << " (expected float32 for dynamic model)\n";
            return;
        }

        int in_elems = 1;
        for (int i = 0; i < in->dims->size; ++i) in_elems *= in->dims->data[i];
        input_size_ = in_elems;

        // Output
        const int out_idx = interpreter_->outputs().empty() ? -1 : interpreter_->outputs()[0];
        if (out_idx < 0) {
            std::cerr << "[TfliteRunner] No output tensor\n";
            return;
        }
        TfLiteTensor* out = interpreter_->tensor(out_idx);
        if (!out) return;

        int out_elems = 1;
        for (int i = 0; i < out->dims->size; ++i) out_elems *= out->dims->data[i];
        output_size_ = out_elems;

        valid_ = true;
    }

    std::vector<float> TfliteRunner::RunFloat(const float* input, int input_size) {
        std::vector<float> out_vec;
        if (!valid_ || !interpreter_) return out_vec;
        if (!input || input_size != input_size_) return out_vec;

        const int in_idx = interpreter_->inputs()[0];
        float* in_ptr = interpreter_->typed_tensor<float>(in_idx);
        if (!in_ptr) return out_vec;

        std::memcpy(in_ptr, input, sizeof(float) * static_cast<size_t>(input_size_));

        if (interpreter_->Invoke() != kTfLiteOk) {
            std::cerr << "[TfliteRunner] Invoke() failed\n";
            return out_vec;
        }

        const int out_idx = interpreter_->outputs()[0];
        const float* out_ptr = interpreter_->typed_tensor<float>(out_idx);
        if (!out_ptr) return out_vec;

        out_vec.assign(out_ptr, out_ptr + output_size_);
        return out_vec;
    }

}  // namespace core::ml
