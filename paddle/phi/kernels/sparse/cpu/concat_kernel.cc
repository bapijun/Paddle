/* Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/phi/kernels/sparse/concat_kernel.h"
#include "paddle/phi/backends/cpu/cpu_context.h"
#include "paddle/phi/core/kernel_registry.h"
#include "paddle/phi/core/meta_tensor.h"
#include "paddle/phi/core/tensor_utils.h"
#include "paddle/phi/infermeta/multiary.h"
#include "paddle/phi/kernels/concat_kernel.h"
#include "paddle/phi/kernels/full_kernel.h"
#include "paddle/phi/kernels/funcs/concat_and_split_functor.h"
#include "paddle/phi/kernels/funcs/concat_funcs.h"

namespace phi {
namespace sparse {

static void check_cat_sparse_dims(const SparseCooTensor* t,
                                  int64_t pos,
                                  DDim dims,
                                  int64_t axis,
                                  int64_t sparse_dim,
                                  int64_t dense_dim) {
  PADDLE_ENFORCE_EQ(t->sparse_dim(),
                    sparse_dim,
                    "All tensors must have the same sparse_dim ",
                    sparse_dim,
                    ", but tensor at position ",
                    pos,
                    " has ",
                    t->sparse_dim());
  PADDLE_ENFORCE_EQ(t->dense_dim(),
                    dense_dim,
                    "All tensors must have the same dense_dim ",
                    dense_dim,
                    ", but tensor at position ",
                    pos,
                    " has ",
                    t->dense_dim());
}

template <typename T>
void ConcatInferMeta(const std::vector<const SparseTensorMeta*>& x,
                     const Scalar& axis_scalar,
                     T* out) {
  PADDLE_ENFORCE_GE(x.size(),
                    0UL,
                    phi::errors::InvalidArgument(
                        "The size of input meta vector should be greater"
                        "than 0."));

  SparseTensorMeta out_meta;
  out_meta.dtype = x.at(0)->dtype;
  out_meta.layout = x.at(0)->layout;
  out->set_meta(out_meta);
}

template <typename T, typename Context>
void ConcatCooKernel(const Context& dev_ctx,
                     const std::vector<const SparseCooTensor*>& x,
                     const Scalar& axis_scalar,
                     SparseCooTensor* out) {
  std::vector<const SparseTensorMeta*> meta_x_ptr;
  meta_x_ptr.reserve(x.size());
  std::vector<DenseTensor> indices;
  std::vector<DenseTensor> values;
  std::vector<phi::DDim> x_dims;

  int64_t axis = axis_scalar.to<int64_t>();
  axis = phi::funcs::ComputeAxis(axis, x[0]->dims().size());
  int32_t sparse_dim = x[0]->sparse_dim();
  int32_t dense_dim = x[0]->dense_dim();

  DDim dims = x[0]->dims();
  DenseTensor out_indices;
  DenseTensor out_values;
  funcs::ConcatFunctor<Context, T> concat_functor;
  int64_t pos = 0;
  for (const auto* t : x) {
    check_cat_sparse_dims(t, pos, dims, axis, sparse_dim, dense_dim);
    meta_x_ptr.push_back(&t->meta());
    x_dims.push_back(t->dims());
    pos++;
  }
  // 把这个方法放到其他的地方
  ConcatInferMeta(meta_x_ptr, axis, out);
  phi::DDim out_dims = phi::funcs::ComputeAndCheckShape(true, x_dims, axis);

  if (axis < sparse_dim) {
    int64_t out_nnz = 0;
    for (const auto* t : x) {
      indices.push_back(t->indices());
      values.push_back(t->values());
      out_nnz = t->nnz();
    }
    DenseTensor out_indices =
        phi::Empty<int64_t, Context>(dev_ctx, {sparse_dim, out_nnz});
    DenseTensor out_values = phi::Empty<T, Context>(dev_ctx, {out_nnz});
    // 因为在前面进行了检查,所以这个维度的nnz都一样
    concat_functor(dev_ctx, indices, static_cast<int>(1), &out_indices);
    concat_functor(dev_ctx, values, static_cast<int>(0), &out_values);
    int64_t col = 0;
    int64_t cumulative_offset = 0;
    auto* out_indices_data = out_indices.data<int64_t>();
    for (size_t i = 0; i != x.size(); i++) {
      int64_t this_piece_size = x[i]->nnz();
      if (i > 0) {
        // indices下会在实际的concat的axis下增加之前的,每一轮下叠加值,
        // 例如针对两个indice [1, 2, 3, 4], [1, 2, 3], [1,
        // 2]进行concat,那么结果就是[1, 2, 3, 4, 1+4, 2+4, 3+4, 4+4, 1+ 4+3,
        // 2+4+3] 这里4和3之前的indice的对应axis的numel
        for (int64_t j = col; j != this_piece_size; j++) {
          // out_nnz = out_indices->dims()[1]
          out_indices_data[axis * out_nnz + j] += cumulative_offset;
        }
      }
      cumulative_offset += x[i]->dims()[axis];
      col += this_piece_size;
    }
    out->SetMember(out_indices, out_values, out_dims, x[0]->coalesced());
  } else {
    int64_t values_dim = axis - sparse_dim + 1;

    int64_t total_size = 0;
    for (auto& r : x) {
      total_size += r->values().dims()[values_dim];
    }
    DDim zeros_sizes = x[0]->values().dims();
    int64_t cumulative_size = 0;

    for (const auto* t : x) {
      zeros_sizes[0] = t->values().dims()[0];
      zeros_sizes[values_dim] = cumulative_size;
      cumulative_size += t->values().dims()[values_dim];
      // z1 z2是全0的向量
      DenseTensor z1 =
          phi::Full<T, Context>(dev_ctx, common::vectorize(zeros_sizes), 0);
      zeros_sizes[values_dim] = total_size - cumulative_size;
      DenseTensor z2 =
          phi::Full<T, Context>(dev_ctx, common::vectorize(zeros_sizes), 0);
      std::vector<DenseTensor> now_values;
      now_values.push_back(z1);
      now_values.push_back(t->values());
      now_values.push_back(z2);
      auto concat_value =
          std::make_shared<DenseTensor>();  // 创建DenseTensor的智能指针
      concat_functor(dev_ctx, now_values, values_dim, concat_value.get());

      values.push_back(*concat_value);
      indices.push_back(t->indices());
    }
    concat_functor(dev_ctx, indices, static_cast<int>(1), &out_indices);
    concat_functor(dev_ctx, values, static_cast<int>(0), &out_values);

    out->SetMember(out_indices, out_values, out_dims, x[0]->coalesced());
  }
}

template <typename T, typename Context>
void ConcatCsrKernel(const Context& dev_ctx,
                     const std::vector<const SparseCsrTensor*>& x,
                     const Scalar& axis_scalar,
                     SparseCsrTensor* out) {
  const size_t num_split = x.size();
  if (num_split == 1) {
    phi::Copy<Context>(dev_ctx, *x[0], dev_ctx.GetPlace(), false, out);
    return;
  }
  std::vector<const SparseTensorMeta*> meta_x_ptr;

  std::vector<phi::DDim> x_dims;
  x_dims.reserve(num_split);

  int64_t axis = axis_scalar.to<int64_t>();
  axis = phi::funcs::ComputeAxis(axis, x[0]->dims().size());

  std::vector<int64_t> crows_numel;
  std::vector<const int64_t*> crows_data_vec;
  std::vector<const T*> values_data_vec;
  std::vector<const int64_t*> cols_data_vec;
  crows_numel.reserve(num_split);
  crows_data_vec.reserve(num_split);
  values_data_vec.reserve(num_split);
  cols_data_vec.reserve(num_split);

  int64_t out_values_size = 0;
  int64_t out_crows_size = 0;
  for (const auto* t : x) {
    // TODO(bapijun) 考虑到nnz = 0的情况,进行补全`
    meta_x_ptr.emplace_back(&t->meta());
    x_dims.emplace_back(t->dims());
    values_data_vec.push_back(t->values().data<T>());
    cols_data_vec.push_back(t->cols().data<int64_t>());
    // nnz == 0 时候,如果crow = [0] 这样的情况,补全0,避免之后的拼接遗漏
    if (t->nnz() == 0 && t->crows().numel() != (t->dims()[0] + 1)) {
      DenseTensor* crows = out->mutable_crows();
      phi::Full<T, Context>(
          dev_ctx, common::vectorize({t->dims()[0] + 1}), 0, crows);
    }
    crows_data_vec.push_back(t->crows().data<int64_t>());
    out_values_size += t->nnz();
  }
  DenseTensor out_values = phi::Empty<T>(dev_ctx, {out_values_size});
  T* out_values_data = out_values.data<T>();
  DenseTensor out_cols = phi::Empty<int64_t>(dev_ctx, {out_values_size});
  int64_t* out_cols_data = out_cols.data<int64_t>();

  // 把这个方法放到其他的地方
  ConcatInferMeta(meta_x_ptr, axis, out);
  phi::DDim out_dims = phi::funcs::ComputeAndCheckShape(true, x_dims, axis);
  int x_dim = x_dims[0].size();
  // now csr only support 2-d and 3-d size
  if (x_dim == 2) {
    if (axis == 0) {
      std::vector<int64_t> nnz_vec;

      // 除了第一个0 之外,按照row的次数叠加
      out_crows_size = 1;
      for (size_t i = 0; i != num_split; i++) {
        // 不过这里会在最开始的填充
        // axis == 0 rows 的大小不一定一样
        size_t rows = static_cast<int>(x[i]->dims()[0]);
        crows_numel.push_back(rows + 1);
        out_crows_size += rows;
        nnz_vec.push_back(x[i]->nnz());
      }

      DenseTensor out_crows = phi::Empty<int64_t>(dev_ctx, {out_crows_size});
      int64_t* out_crows_data = out_crows.data<int64_t>();

      // concat_functor(dev_ctx, values, static_cast<int>(0), &out_values);
      // 替换掉方便编写的方法
      int64_t value_offset = 0;
      for (size_t i = 0; i != num_split; i++) {
        int nnz = nnz_vec[i];
        // nnz == 0 的特殊情况,此时out_values_data指针很可能是错误的
        std::memcpy(out_values_data + value_offset,
                    values_data_vec[i],
                    nnz * sizeof(T));
        std::memcpy(out_cols_data + value_offset,
                    cols_data_vec[i],
                    nnz * sizeof(int64_t));

        value_offset += nnz;
      }
      // rows_in_slice 保存的是每一个crows的和,方便concat的计算 多给1条方便优化

      // 原始的代码
      // 在cuda中优化?
      // 第一位初始化
      out_crows_data[0] = 0;
      // crows_offset 表示已经计算完的一轮crows_offset
      int64_t crows_offset = 0;
      for (size_t i = 0; i != num_split; i++) {
        for (int64_t j = 1; j != crows_numel[i]; j++) {
          // 每轮循环crows_numel[i] - 1(也就是对应的行数)次
          // 对于crows.lenght 的序列[[x], [y], [z] ...]
          // 针对输出的情况out_crow应该是[x, y-1, z-1]
          // ,同时y-1,z-1...这些段落要加上之前的叠加和
          // i = crows_offset = 0
          out_crows_data[j] = crows_data_vec[i][j] + crows_offset;
        }
        // nnz == 0的情况也不会处触发这里的计算,这时候crows_numel[*]==0
        crows_offset += crows_data_vec[i][crows_numel[i]];
      }
      out->SetMember(out_crows, out_cols, out_values, out_dims);
    } else {  // axis == 1
      // num_split >= 2
      // 除了最后一位 crow_numel 列数都相等
      size_t rows = static_cast<size_t>(x[0]->dims()[0]);
      out_crows_size = rows + 1;

      DenseTensor out_crows = phi::Empty<int64_t>(dev_ctx, {out_crows_size});
      int64_t* out_crows_data = out_crows.data<int64_t>();

      int64_t out_cols_offset = 0;
      out_crows_data[0] = 0;
      int now_cols_offset = 0;
      for (int64_t j = 1; j < out_crows_size; j++) {
        for (size_t i = 0; i != num_split; i++) {
          now_cols_offset = 0;
          for (int64_t k = 0; crows_data_vec[i][j] - crows_data_vec[i][j - 1];
               k++) {
            // 针对col需要添加之前的列数作为额外的offset
            out_cols_data[out_cols_offset] =
                cols_data_vec[i][now_cols_offset] + i * rows;
            out_values_data[out_cols_offset] =
                values_data_vec[i][now_cols_offset];
            now_cols_offset++;
            out_cols_offset++;
          }
          out_crows_data[j] = out_cols_offset;
        }
      }
      out->SetMember(out_crows, out_cols, out_values, out_dims);
    }

  } else {
    // dim==3
    if (axis == 0) {
      std::vector<DenseTensor> crows;
      std::vector<DenseTensor> values;
      std::vector<DenseTensor> cols;

      for (size_t i = 0; i != num_split; i++) {
        out_crows_size += crows_numel[i];
        crows.emplace_back(x[i]->crows());
        values.emplace_back(x[i]->values());
        cols.emplace_back(x[i]->cols());
      }
      // axis==0 简单拼接所有的三个即可即可完成
      funcs::ConcatFunctor<Context, T> concat_functor;
      concat_functor(dev_ctx, values, static_cast<int>(0), &out_values);
      // cols的形状与value一致
      concat_functor(dev_ctx, cols, static_cast<int>(0), &out_cols);

      DenseTensor out_crows = phi::Empty<int64_t>(dev_ctx, {out_crows_size});

      concat_functor(dev_ctx, crows, static_cast<int>(0), &out_crows);
      out->SetMember(out_crows, out_cols, out_values, out_dims);
    } else if (axis == 1) {
      // 对于dim == 1的情况类似于拆分到2d下dim=0的情况
      // 对于dim==1 的情况下batch必然要一致
      size_t batch = static_cast<int>(x[0]->dims()[0]);
      std::vector<int64_t> nnz_vec;
      for (size_t b = 0; b < batch; b++) {
        out_crows_size += 1;
        for (size_t i = 0; i != num_split; i++) {
          int64_t rows = static_cast<int64_t>(x[i]->dims()[1]);
          crows_numel.push_back(rows + 1);
          out_crows_size += rows;
        }
      }

      DenseTensor out_crows = phi::Empty<int64_t>(dev_ctx, {out_crows_size});
      int64_t* out_crows_data = out_crows.data<int64_t>();

      std::vector<int64_t> values_index(batch + 1, 0);
      int64_t value_offset = 0, crow_index = 0, now_crow_offset = 0;
      for (size_t b = 0; b < batch; b++) {
        // 针对每一轮batch的初始化
        nnz_vec.clear();
        crows_data_vec.clear();
        values_data_vec.clear();
        cols_data_vec.clear();
        for (size_t i = 0; i != num_split; i++) {
          const int64_t* x_crows_ptr = x[i]->crows().data<int64_t>();
          int64_t x_crows_nnz = x_crows_ptr[(b + 1) * (crows_numel[i]) - 1];
          nnz_vec.push_back(x_crows_nnz);
          crows_data_vec.push_back(out_crows_data + b * crows_numel[i]);
          values_data_vec.push_back(out_values_data + values_index[i]);
          cols_data_vec.push_back(out_cols_data + values_index[i]);
          values_index[i + 1] += x_crows_nnz;
        }

        for (size_t i = 0; i != num_split; i++) {
          int nnz = nnz_vec[i];
          if (nnz) {
            // nnz == 0 的特殊情况,此时out_values_data指针很可能是错误的
            std::memcpy(out_values_data + value_offset,
                        values_data_vec[i],
                        nnz * sizeof(T));
            std::memcpy(out_cols_data + value_offset,
                        cols_data_vec[i],
                        nnz * sizeof(int64_t));
          }

          value_offset += nnz;
        }
        out_crows_data[crow_index] = 0;
        crow_index++;
        now_crow_offset = 0;
        for (size_t i = 0; i != num_split; i++) {
          for (int64_t j = 1; j != crows_numel[i]; j++) {
            out_crows_data[crow_index] = crows_data_vec[i][j] + now_crow_offset;
            crow_index++;
          }
          // nnz == 0的情况也不会阴险这里的计算,这时候crows_numel[*]==0
          now_crow_offset += crows_data_vec[i][crows_numel[i]];
        }
      }
      out->SetMember(out_crows, out_cols, out_values, out_dims);
    } else {  // axis = 2
              // axis == 2的情况类似于拆分到2d下axis=1的情况
              // 对于concat axis = 2的情况下,batch下 row的大小都一致
      size_t batch = static_cast<int>(x[0]->dims()[0]);
      size_t rows = static_cast<int>(x[0]->dims()[1]);
      int64_t now_crow_numel = rows + 1;

      out_crows_size = now_crow_numel * batch;
      DenseTensor out_crows = phi::Empty<int64_t>(dev_ctx, {out_crows_size});
      int64_t* out_crows_data = out_crows.data<int64_t>();

      std::vector<int64_t> values_index(num_split + 1, 0);
      int64_t out_cols_offset = 0, crow_offset = 0, batch_out_cols_offset = 0;
      for (size_t b = 0; b < batch; b++) {
        crows_data_vec.clear();
        values_data_vec.clear();
        cols_data_vec.clear();
        batch_out_cols_offset = 0;
        int now_cols_offset = 0;
        for (size_t i = 0; i != num_split; i++) {
          const int64_t* x_crows_ptr = x[i]->crows().data<int64_t>();
          // x_crows_nnz 为当前的二维数组下的nnz
          int64_t x_crows_nnz = x_crows_ptr[(b + 1) * (now_crow_numel)-1];

          crows_data_vec.push_back(out_crows_data + b * now_crow_numel);
          values_data_vec.push_back(out_values_data + values_index[i]);
          cols_data_vec.push_back(out_cols_data + values_index[i]);
          // values_index[i] 表示当前batch下
          // 当前维度下在对应的out_values开始位置和起始位置的差值
          values_index[i + 1] += x_crows_nnz;
        }
        for (int i = 0; i < now_crow_numel; ++i) {
          out_crows_data[i + crow_offset] = 0;
        }

        int64_t row = now_crow_numel - 1;
        for (int64_t j = 1; j < now_crow_numel; j++) {
          for (size_t i = 0; i != num_split; i++) {
            now_cols_offset = 0;
            for (int64_t k = 0; crows_data_vec[i][j] - crows_data_vec[i][j - 1];
                 k++) {
              // 针对col需要添加之前的列数作为额外的offset
              out_cols_data[out_cols_offset] =
                  cols_data_vec[i][now_cols_offset] + i * row;
              out_values_data[out_cols_offset] =
                  values_data_vec[i][now_cols_offset];
              now_cols_offset++;
              out_cols_offset++;
              batch_out_cols_offset++;
            }
            out_crows_data[j + crow_offset] = batch_out_cols_offset;
          }
        }
      }
      out->SetMember(out_crows, out_cols, out_values, out_dims);
    }
  }
}

}  // namespace sparse
}  // namespace phi

PD_REGISTER_KERNEL(concat_coo,
                   CPU,
                   ALL_LAYOUT,
                   phi::sparse::ConcatCooKernel,
                   float,
                   double,
                   bool,
                   int64_t,
                   int,
                   uint8_t,
                   int8_t,
                   int16_t,
                   phi::dtype::float16,
                   phi::dtype::bfloat16,
                   phi::dtype::complex<float>,
                   phi::dtype::complex<double>) {}

PD_REGISTER_KERNEL(concat_csr,
                   CPU,
                   ALL_LAYOUT,
                   phi::sparse::ConcatCsrKernel,
                   float,
                   double,
                   bool,
                   int64_t,
                   int,
                   uint8_t,
                   int8_t,
                   int16_t) {}
