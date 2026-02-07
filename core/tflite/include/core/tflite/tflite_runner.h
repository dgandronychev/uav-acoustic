#pragma once

#include <memory>
#include <string>
#include <vector>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"

namespace core::ml {

	class TfliteRunner {
	public:
		explicit TfliteRunner(const std::string& model_path);

		bool IsValid() const { return valid_; }

		// input: pointer to contiguous float buffer (dynamic-quant model expects float32)
		// returns: output vector (logits or probabilities; depends on the model)
		std::vector<float> RunFloat(const float* input, int input_size);

		int input_size() const { return input_size_; }
		int output_size() const { return output_size_; }

	private:
		bool valid_{ false };

		std::unique_ptr<tflite::FlatBufferModel> model_;
		std::unique_ptr<tflite::Interpreter> interpreter_;

		int input_size_{ 0 };
		int output_size_{ 0 };
	};

}  // namespace core::ml
