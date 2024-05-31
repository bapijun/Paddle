#   Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# TODO: import all neural network related api under this directory,
# including layers, linear, conv, rnn etc.

from .activation import (
    celu,
    elu,
    elu_,
    gelu,
    glu,
    gumbel_softmax,
    hardshrink,
    hardsigmoid,
    hardswish,
    hardtanh,
    hardtanh_,
    leaky_relu,
    leaky_relu_,
    log_sigmoid,
    log_softmax,
    maxout,
    mish,
    prelu,
    relu,
    relu6,
    relu_,
    rrelu,
    selu,
    sigmoid,
    silu,
    softmax,
    softmax_,
    softplus,
    softshrink,
    softsign,
    swish,
    tanh,
    tanh_,
    tanhshrink,
    thresholded_relu,
    thresholded_relu_,
)
from .common import (
    alpha_dropout,
    bilinear,
    class_center_sample,
    cosine_similarity,
    dropout,
    dropout2d,
    dropout3d,
    fold,
    interpolate,
    label_smooth,
    linear,
    pad,
    unfold,
    upsample,
    zeropad2d,
)
from .conv import (
    conv1d,
    conv1d_transpose,
    conv2d,
    conv2d_transpose,
    conv3d,
    conv3d_transpose,
)
from .distance import pairwise_distance, pdist  # noqa: F401
from .extension import (
    diag_embed,  # noqa: F401
    gather_tree,
    sequence_mask,
    temporal_shift,
)
from .flash_attention import (
    flash_attention_with_sparse_mask,
    flash_attn_qkvpacked,
    flash_attn_varlen_qkvpacked,
    scaled_dot_product_attention,
    sdp_kernel,  # noqa: F401
)
from .input import embedding, one_hot
from .loss import (
    adaptive_log_softmax_with_loss,
    binary_cross_entropy,
    binary_cross_entropy_with_logits,
    cosine_embedding_loss,
    cross_entropy,
    ctc_loss,
    dice_loss,
    gaussian_nll_loss,
    hinge_embedding_loss,
    hsigmoid_loss,
    kl_div,
    l1_loss,
    log_loss,
    margin_cross_entropy,
    margin_ranking_loss,
    mse_loss,
    multi_label_soft_margin_loss,
    multi_margin_loss,
    nll_loss,
    npair_loss,
    poisson_nll_loss,
    rnnt_loss,
    sigmoid_focal_loss,
    smooth_l1_loss,
    soft_margin_loss,
    softmax_with_cross_entropy,
    square_error_cost,
    triplet_margin_loss,
    triplet_margin_with_distance_loss,
)
from .norm import (
    batch_norm,
    group_norm,
    instance_norm,
    layer_norm,
    local_response_norm,
    normalize,
)
from .pooling import (
    adaptive_avg_pool1d,
    adaptive_avg_pool2d,
    adaptive_avg_pool3d,
    adaptive_max_pool1d,
    adaptive_max_pool2d,
    adaptive_max_pool3d,
    avg_pool1d,
    avg_pool2d,
    avg_pool3d,
    fractional_max_pool2d,
    fractional_max_pool3d,
    max_pool1d,
    max_pool2d,
    max_pool3d,
    max_unpool1d,
    max_unpool2d,
    max_unpool3d,
)
from .sparse_attention import sparse_attention
from .vision import (
    affine_grid,
    channel_shuffle,
    grid_sample,
    pixel_shuffle,
    pixel_unshuffle,
)

__all__ = [
    'celu',
    'conv1d',
    'conv1d_transpose',
    'conv2d',
    'conv2d_transpose',
    'conv3d',
    'conv3d_transpose',
    'pairwise_distance',
    'elu',
    'elu_',
    'gelu',
    'hardshrink',
    'hardtanh',
    'hardtanh_',
    'hardsigmoid',
    'hardswish',
    'leaky_relu',
    'leaky_relu_',
    'log_sigmoid',
    'maxout',
    'prelu',
    'relu',
    'relu_',
    'relu6',
    'selu',
    'softmax',
    'softmax_',
    'softplus',
    'softshrink',
    'softsign',
    'sigmoid',
    'silu',
    'swish',
    'mish',
    'tanh',
    'tanh_',
    'tanhshrink',
    'thresholded_relu',
    'thresholded_relu_',
    'log_softmax',
    'glu',
    'gumbel_softmax',
    'sequence_mask',
    'dropout',
    'dropout2d',
    'dropout3d',
    'alpha_dropout',
    'label_smooth',
    'linear',
    'pad',
    'zeropad2d',
    'unfold',
    'interpolate',
    'upsample',
    'bilinear',
    'cosine_similarity',
    'avg_pool1d',
    'avg_pool2d',
    'avg_pool3d',
    'max_pool1d',
    'max_pool2d',
    'max_pool3d',
    'max_unpool1d',
    'max_unpool2d',
    'max_unpool3d',
    'adaptive_avg_pool1d',
    'adaptive_avg_pool2d',
    'adaptive_avg_pool3d',
    'adaptive_max_pool1d',
    'adaptive_max_pool2d',
    'adaptive_max_pool3d',
    'fractional_max_pool2d',
    'fractional_max_pool3d',
    'binary_cross_entropy',
    'binary_cross_entropy_with_logits',
    'cross_entropy',
    'dice_loss',
    'hsigmoid_loss',
    'kl_div',
    'l1_loss',
    'log_loss',
    'mse_loss',
    'margin_ranking_loss',
    'multi_label_soft_margin_loss',
    'nll_loss',
    'poisson_nll_loss',
    'npair_loss',
    'sigmoid_focal_loss',
    'smooth_l1_loss',
    'softmax_with_cross_entropy',
    'margin_cross_entropy',
    'square_error_cost',
    'ctc_loss',
    'rnnt_loss',
    'hinge_embedding_loss',
    'affine_grid',
    'grid_sample',
    'local_response_norm',
    'pixel_shuffle',
    'pixel_unshuffle',
    'channel_shuffle',
    'embedding',
    'gather_tree',
    'one_hot',
    'normalize',
    'temporal_shift',
    'batch_norm',
    'layer_norm',
    'instance_norm',
    'class_center_sample',
    'sparse_attention',
    'fold',
    'cosine_embedding_loss',
    'rrelu',
    'triplet_margin_with_distance_loss',
    'triplet_margin_loss',
    'adaptive_log_softmax_with_loss',
    'multi_margin_loss',
    'soft_margin_loss',
    'gaussian_nll_loss',
    'scaled_dot_product_attention',
    'flash_attention_with_sparse_mask',
    'flash_attn_qkvpacked',
    'flash_attn_varlen_qkvpacked',
    'group_norm',
]
