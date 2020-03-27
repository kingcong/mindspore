/**
 * Copyright 2019 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "transform/op_adapter_util.h"

#include <string>
#include <vector>
#include <algorithm>

#include "utils/utils.h"
#include "transform/op_adapter_base.h"

namespace mindspore {
namespace transform {
GeTensor ConvertAnyUtil(const ValuePtr& value, const AnyTraits<mindspore::tensor::Tensor>&) {
  // To-DO the format may read from ME tensor
  MS_EXCEPTION_IF_NULL(value);
  auto me_tensor = value->cast<MeTensorPtr>();
  auto ge_tensor = TransformUtil::ConvertTensor(me_tensor, kOpFormat_NCHW);
  return ge_tensor == nullptr ? GeTensor() : *ge_tensor;
}

std::vector<int64_t> ConvertAnyUtil(const ValuePtr& value, const std::string& name,
                                    const AnyTraits<std::vector<int64_t>>) {
  int64_t data = GetValue<int>(value);
  std::vector<int64_t> list;
  int size = 2;  // 2 int in list
  if (name == "pad") {
    size = 4;  // 4 int in list
    list = TransformUtil::ConvertIntToList(data, size);
    list[0] = 1;
    list[1] = 1;
  } else {
    list = TransformUtil::ConvertIntToList(data, size);
  }

  return list;
}

std::string ConvertAnyUtil(const ValuePtr& value, const AnyTraits<std::vector<int64_t>>, const AnyTraits<std::string>) {
  MS_EXCEPTION_IF_NULL(value);
  auto vec = value->cast<ValueTuplePtr>();
  if (nullptr == vec) {
    MS_LOG(EXCEPTION) << "not ValueTuplePtr";
  }
  std::ostringstream buffer;
  int i = 0;
  for (auto& it : vec->value()) {
    if (i != 0) {
      buffer << ",";
    }
    buffer << GetValue<int>(it);
    i++;
  }
  return buffer.str();
}

std::vector<float> ConvertAnyUtil(const ValuePtr& value, const AnyTraits<std::vector<float>>, const AnyTraits<float>) {
  MS_EXCEPTION_IF_NULL(value);
  auto vec = value->cast<ValueTuplePtr>();
  if (nullptr == vec) {
    MS_LOG(EXCEPTION) << "not ValueTuplePtr";
  }
  std::vector<float> list;
  list.resize(vec->value().size());
  (void)std::transform(vec->value().begin(), vec->value().end(), list.begin(),
                       [](const ValuePtr& val) { return static_cast<float>(GetValue<float>(val)); });
  return list;
}

std::vector<int64_t> ConvertAnyUtil(const ValuePtr& value, const std::string& format,
                                    const AnyTraits<std::vector<int64_t>>, const AnyTraits<int64_t>) {
  MS_EXCEPTION_IF_NULL(value);
  auto vec = value->cast<ValueTuplePtr>();
  if (nullptr == vec) {
    MS_LOG(EXCEPTION) << "not ValueTuplePtr";
  }
  std::vector<int64_t> list;
  list.resize(vec->value().size());
  (void)std::transform(vec->value().begin(), vec->value().end(), list.begin(),
                       [](const ValuePtr& val) { return static_cast<int64_t>(GetValue<int>(val)); });
  if (format == kOpFormat_NHWC) {
    if (list.size() < 4) {
      MS_LOG(EXCEPTION) << "The size of list is less than 4";
    } else {
      int64_t temp = list[1];
      list[1] = list[2];
      list[2] = list[3];
      list[3] = temp;
    }
  }
  return list;
}

GeDataType ConvertAnyUtil(const ValuePtr& value, const AnyTraits<GEType>) {
  MS_EXCEPTION_IF_NULL(value);
  if (!value->isa<Type>()) {
    MS_LOG(EXCEPTION) << "error convert Value to TypePtr for value: " << value->ToString()
                      << ", type: " << value->type_name() << ", value should be a Typeptr";
  }
  auto type = value->cast<TypePtr>();
  MS_EXCEPTION_IF_NULL(type);
  TypeId me_type = type->type_id();
  if (kObjectTypeTensorType == me_type) {
    me_type = dyn_cast<TensorType>(type)->element()->type_id();
  }
  return TransformUtil::ConvertDataType(me_type);
}

GeTensor VectorToTensorUtil(const ValuePtr& value) {
  // convert tuple or list to ge tensor, only supported one dim for now
  MS_EXCEPTION_IF_NULL(value);
  auto vec = value->isa<ValueTuple>() ? value->cast<ValueTuplePtr>()->value() : value->cast<ValueListPtr>()->value();
  if (vec.empty()) {
    MS_LOG(WARNING) << "Convert a none tuple to an empty ge tensor";
    return GeTensor();
  }
  MS_EXCEPTION_IF_NULL(vec[0]);
  if (vec[0]->isa<Int32Imm>()) {
    MS_LOG(INFO) << "convert value to tensor with data type = Int32";
    auto data = ConvertAnyUtil(value, AnyTraits<int32_t>(), AnyTraits<std::vector<int32_t>>());
    auto desc = TransformUtil::GetGeTensorDesc({static_cast<int>(vec.size())}, kNumberTypeInt32, kOpFormat_NCHW);
    if (desc == nullptr) {
      MS_LOG(EXCEPTION) << "Update conversion descriptor failed!";
    }
    return GeTensor(*desc, reinterpret_cast<uint8_t*>(data.data()), data.size() * sizeof(int32_t));
  } else if (vec[0]->isa<FP32Imm>()) {
    MS_LOG(INFO) << "convert value to tensor with data type = Float32";
    auto data = ConvertAnyUtil(value, AnyTraits<float>(), AnyTraits<std::vector<float>>());
    auto desc = TransformUtil::GetGeTensorDesc({static_cast<int>(vec.size())}, kNumberTypeFloat32, kOpFormat_NCHW);
    if (desc == nullptr) {
      MS_LOG(EXCEPTION) << "Update conversion descriptor failed!";
    }
    return GeTensor(*desc, reinterpret_cast<uint8_t*>(data.data()), data.size() * sizeof(float));
  } else if (vec[0]->isa<BoolImm>()) {
    MS_LOG(INFO) << "convert value to tensor with data type = Bool";
    // We use uint8_t to save bool type data
    auto data = ConvertAnyUtil(value, AnyTraits<bool>(), AnyTraits<std::vector<uint8_t>>());
    auto desc = TransformUtil::GetGeTensorDesc({static_cast<int>(vec.size())}, kNumberTypeBool, kOpFormat_NCHW);
    if (desc == nullptr) {
      MS_LOG(EXCEPTION) << "Update conversion descriptor failed!";
    }
    return GeTensor(*desc, static_cast<uint8_t*>(data.data()), data.size() * sizeof(uint8_t));
  } else {
    MS_LOG(EXCEPTION) << "Unsupported data type of tuple or list elements: " << vec[0]->type_name();
  }

  return GeTensor();
}

GeTensor ConvertAnyUtil(const ValuePtr& value, const AnyTraits<AnyValue>) {
  MS_EXCEPTION_IF_NULL(value);
  if (value->isa<MeTensor>()) {
    // convert me tensor to ge tensor
    return ConvertAnyUtil(value, AnyTraits<MeTensor>());
  } else if (value->isa<ValueList>() || value->isa<ValueTuple>()) {
    return VectorToTensorUtil(value);
  } else if (value->isa<Int32Imm>()) {
    // convert scalar Int to GeTensor
    MS_LOG(INFO) << "convert scalar to tensor with data type = Int32";
    GeTensorDesc desc(GeShape(), ge::FORMAT_NCHW, ge::DT_INT32);
    auto v = GetValue<int32_t>(value);
    desc.SetRealDimCnt(0);
    return GeTensor(desc, reinterpret_cast<uint8_t*>(&v), sizeof(int32_t));
  } else if (value->isa<Int64Imm>()) {
    // convert scalar Int64 to GeTensor
    MS_LOG(INFO) << "convert scalar to tensor with data type = Int64";
    GeTensorDesc desc(GeShape(), ge::FORMAT_NCHW, ge::DT_INT64);
    auto v = GetValue<int64_t>(value);
    desc.SetRealDimCnt(0);
    return GeTensor(desc, reinterpret_cast<uint8_t*>(&v), sizeof(int64_t));
  } else if (value->isa<FP32Imm>()) {
    // convert scalar FP32 to GeTensor
    MS_LOG(INFO) << "convert scalar to tensor with data type = FP32";
    GeTensorDesc desc(GeShape(), ge::FORMAT_NCHW, ge::DT_FLOAT);
    auto v = GetValue<float>(value);
    desc.SetRealDimCnt(0);
    return GeTensor(desc, reinterpret_cast<uint8_t*>(&v), sizeof(float));
  } else if (value->isa<BoolImm>()) {
    // convert scalar FP32 to GeTensor
    MS_LOG(INFO) << "convert scalar to tensor with data type = Bool";
    GeTensorDesc desc(GeShape(), ge::FORMAT_NCHW, ge::DT_BOOL);
    auto v = GetValue<bool>(value);
    desc.SetRealDimCnt(0);
    return GeTensor(desc, reinterpret_cast<uint8_t*>(&v), sizeof(bool));
  } else if (value->isa<StringImm>()) {
    // convert String to GeTensor
    MS_LOG(INFO) << "convert string to tensor with data type = String";
    std::string v = GetValue<std::string>(value);
    std::vector<int64_t> ge_shape;
    GeShape shape(ge_shape);
    GeTensorDesc desc(shape, ge::FORMAT_NCHW, ge::DT_STRING);
    GeTensor str_tensor(desc);
    str_tensor.SetData(v);
    return str_tensor;
  } else {
    MS_LOG(WARNING) << "Unsupported value type: " << value->type_name()
                    << " to convert to tensor. Value: " << value->ToString();
  }
  return GeTensor();
}

bool IsCustomPrim(const PrimitivePtr& prim) {
  if (prim == nullptr) {
    return false;
  }

  ValuePtr flag = prim->GetAttr("_custom_op_flag");
  if (flag == nullptr) {
    return false;
  }

  return GetValue<bool>(flag);
}

bool IsCustomCNode(const AnfNodePtr& anf) {
  if (anf == nullptr) {
    return false;
  }
  auto node = anf->cast<CNodePtr>();
  if (node == nullptr) {
    return false;
  }
  if (node->inputs().empty()) {
    MS_LOG(EXCEPTION) << "length of node inputs is empty";
  }
  MS_EXCEPTION_IF_NULL(node->inputs()[0]);
  if (!node->inputs()[0]->isa<ValueNode>()) {
    return false;
  }
  auto cus_prim = GetValueNode<PrimitivePtr>(node->inputs()[0]);
  if (cus_prim == nullptr) {
    return false;
  }

  return IsCustomPrim(cus_prim);
}
}  // namespace transform
}  // namespace mindspore