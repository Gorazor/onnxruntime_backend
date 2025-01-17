// Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <onnxruntime_c_api.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "triton/backend/backend_common.h"
#include "triton/core/tritonserver.h"

namespace triton { namespace backend { namespace onnxruntime {

extern const OrtApi* ort_api;

#define RESPOND_ALL_AND_RETURN_IF_ERROR(                          \
    REQUESTS, REQUEST_COUNT, RESPONSES, S)                        \
  do {                                                            \
    TRITONSERVER_Error* raarie_err__ = (S);                       \
    if (raarie_err__ != nullptr) {                                \
      for (uint32_t r = 0; r < REQUEST_COUNT; ++r) {              \
        TRITONBACKEND_Response* response = (*RESPONSES)[r];       \
        if (response != nullptr) {                                \
          LOG_IF_ERROR(                                           \
              TRITONBACKEND_ResponseSend(                         \
                  response, TRITONSERVER_RESPONSE_COMPLETE_FINAL, \
                  raarie_err__),                                  \
              "failed to send ONNXRuntime backend response");     \
          response = nullptr;                                     \
        }                                                         \
        LOG_IF_ERROR(                                             \
            TRITONBACKEND_RequestRelease(                         \
                REQUESTS[r], TRITONSERVER_REQUEST_RELEASE_ALL),   \
            "failed releasing request");                          \
        REQUESTS[r] = nullptr;                                    \
      }                                                           \
      TRITONSERVER_ErrorDelete(raarie_err__);                     \
      return;                                                     \
    }                                                             \
  } while (false) 

#define RESPOND_ALL_AND_RETURN_IF_ORT_ERROR(                                      \
    REQUESTS, REQUEST_COUNT, RESPONSES, S)                                        \
  do {                                                                            \
    OrtStatus* status__ = (S);                                                    \
    if (status__ != nullptr) {                                                    \
      OrtErrorCode code = ort_api->GetErrorCode(status__);                        \
      std::string msg = std::string(ort_api->GetErrorMessage(status__));          \
      ort_api->ReleaseStatus(status__);                                           \
      auto err__ = TRITONSERVER_ErrorNew(                                         \
              TRITONSERVER_ERROR_INTERNAL,                                        \
              (std::string("onnx runtime error ") + std::to_string(code) +        \
               ": " + msg)                                                        \
                  .c_str());                                                      \
      RESPOND_ALL_AND_RETURN_IF_ERROR(REQUESTS, REQUEST_COUNT, RESPONSES, err__); \
    }                                                                             \
  } while (false)

#define RETURN_IF_ORT_ERROR(S)                                               \
  do {                                                                       \
    OrtStatus* status__ = (S);                                               \
    if (status__ != nullptr) {                                               \
      OrtErrorCode code = ort_api->GetErrorCode(status__);                   \
      std::string msg = std::string(ort_api->GetErrorMessage(status__));     \
      ort_api->ReleaseStatus(status__);                                      \
      return TRITONSERVER_ErrorNew(                                          \
          TRITONSERVER_ERROR_INTERNAL, (std::string("onnx runtime error ") + \
                                        std::to_string(code) + ": " + msg)   \
                                           .c_str());                        \
    }                                                                        \
  } while (false)                    

#define THROW_IF_BACKEND_MODEL_ORT_ERROR(S)                                  \
  do {                                                                       \
    OrtStatus* status__ = (S);                                               \
    if (status__ != nullptr) {                                               \
      OrtErrorCode code = ort_api->GetErrorCode(status__);                   \
      std::string msg = std::string(ort_api->GetErrorMessage(status__));     \
      ort_api->ReleaseStatus(status__);                                      \
      throw BackendModelException(TRITONSERVER_ErrorNew(                     \
          TRITONSERVER_ERROR_INTERNAL, (std::string("onnx runtime error ") + \
                                        std::to_string(code) + ": " + msg)   \
                                           .c_str()));                       \
    }                                                                        \
  } while (false)

#define THROW_IF_BACKEND_INSTANCE_ORT_ERROR(S)                               \
  do {                                                                       \
    OrtStatus* status__ = (S);                                               \
    if (status__ != nullptr) {                                               \
      OrtErrorCode code = ort_api->GetErrorCode(status__);                   \
      std::string msg = std::string(ort_api->GetErrorMessage(status__));     \
      ort_api->ReleaseStatus(status__);                                      \
      throw BackendModelInstanceException(TRITONSERVER_ErrorNew(             \
          TRITONSERVER_ERROR_INTERNAL, (std::string("onnx runtime error ") + \
                                        std::to_string(code) + ": " + msg)   \
                                           .c_str()));                       \
    }                                                                        \
  } while (false)

struct OnnxTensorInfo {
  OnnxTensorInfo(ONNXTensorElementDataType type, std::vector<int64_t> dims)
      : type_(type), dims_(dims)
  {
  }

  ONNXTensorElementDataType type_;
  std::vector<int64_t> dims_;
};

using OnnxTensorInfoMap = std::unordered_map<std::string, OnnxTensorInfo>;

/// Deleter for OrtTypeInfo.
struct TypeInfoDeleter {
  void operator()(OrtTypeInfo* f) { ort_api->ReleaseTypeInfo(f); }
};

/// Deleter for OrtSessionOptions.
struct SessionOptionsDeleter {
  void operator()(OrtSessionOptions* f) { ort_api->ReleaseSessionOptions(f); }
};

std::string OnnxDataTypeName(ONNXTensorElementDataType onnx_type);

TRITONSERVER_DataType ConvertFromOnnxDataType(
    ONNXTensorElementDataType onnx_type);

ONNXTensorElementDataType ConvertToOnnxDataType(
    TRITONSERVER_DataType data_type);
ONNXTensorElementDataType ConvertToOnnxDataType(
    const std::string& data_type_str);

ONNXTensorElementDataType ModelConfigDataTypeToOnnxDataType(
    const std::string& data_type_str);
std::string OnnxDataTypeToModelConfigDataType(
    ONNXTensorElementDataType data_type);

TRITONSERVER_Error* InputNames(
    OrtSession* session, std::set<std::string>& names);
TRITONSERVER_Error* OutputNames(
    OrtSession* session, std::set<std::string>& names);

TRITONSERVER_Error* InputInfos(
    OrtSession* session, OrtAllocator* allocator, OnnxTensorInfoMap& infos);
TRITONSERVER_Error* OutputInfos(
    OrtSession* session, OrtAllocator* allocator, OnnxTensorInfoMap& infos);

TRITONSERVER_Error* CompareDimsSupported(
    const std::string& model_name, const std::string& tensor_name,
    const std::vector<int64_t>& model_shape, const std::vector<int64_t>& dims,
    const int max_batch_size, const bool compare_exact);

}}}  // namespace triton::backend::onnxruntime
