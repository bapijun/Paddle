# Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
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

import unittest

import numpy as np
from op_test import OpTest, convert_float_to_uint16

import paddle
from paddle import base
from paddle.base import core
from paddle.pir_utils import test_with_pir_api


class TestCrossOp(OpTest):
    def setUp(self):
        self.op_type = "cross"
        self.python_api = paddle.cross
        self.initTestCase()
        self.inputs = {
            'X': np.random.random(self.shape).astype(self.dtype),
            'Y': np.random.random(self.shape).astype(self.dtype),
        }
        self.init_output()

    def initTestCase(self):
        self.attrs = {'dim': -2}
        self.dtype = np.float64
        self.shape = (1024, 3, 1)

    def init_output(self):
        x = np.squeeze(self.inputs['X'], 2)
        y = np.squeeze(self.inputs['Y'], 2)
        z_list = []
        for i in range(1024):
            z_list.append(np.cross(x[i], y[i]))
        self.outputs = {'Out': np.array(z_list).reshape(self.shape)}

    def test_check_output(self):
        self.check_output(check_pir=True)

    def test_check_grad_normal(self):
        self.check_grad(['X', 'Y'], 'Out', check_pir=True)


class TestCrossOpCase1(TestCrossOp):
    def initTestCase(self):
        self.shape = (2048, 3)
        self.dtype = np.float32

    def init_output(self):
        z_list = []
        for i in range(2048):
            z_list.append(np.cross(self.inputs['X'][i], self.inputs['Y'][i]))
        self.outputs = {'Out': np.array(z_list).reshape(self.shape)}


@unittest.skipIf(
    not core.is_compiled_with_cuda(), "core is not compiled with CUDA"
)
class TestCrossFP16Op(TestCrossOp):
    def initTestCase(self):
        self.shape = (2048, 3)
        self.dtype = np.float16

    def init_output(self):
        z_list = []
        for i in range(2048):
            z_list.append(np.cross(self.inputs['X'][i], self.inputs['Y'][i]))
        self.outputs = {'Out': np.array(z_list).reshape(self.shape)}



# 在这里写一个两组复数的版本,然后添加对应的代码 cpu和gpu



@unittest.skipIf(
    not core.is_compiled_with_cuda()
    or not core.is_bfloat16_supported(core.CUDAPlace(0)),
    "core is not compiled with CUDA and not support the bfloat16",
)
class TestCrossBF16Op(OpTest):
    def setUp(self):
        self.op_type = "cross"
        self.python_api = paddle.cross
        self.initTestCase()
        self.x = np.random.random(self.shape).astype(np.float32)
        self.y = np.random.random(self.shape).astype(np.float32)
        self.inputs = {
            'X': convert_float_to_uint16(self.x),
            'Y': convert_float_to_uint16(self.y),
        }
        self.init_output()

    def initTestCase(self):
        self.attrs = {'dim': -2}
        self.dtype = np.uint16
        self.shape = (1024, 3, 1)

    def init_output(self):
        x = np.squeeze(self.x, 2)
        y = np.squeeze(self.y, 2)
        z_list = []
        for i in range(1024):
            z_list.append(np.cross(x[i], y[i]))
        out = np.array(z_list).astype(np.float32).reshape(self.shape)
        self.outputs = {'Out': convert_float_to_uint16(out)}

    def test_check_output(self):
        if core.is_compiled_with_cuda():
            place = core.CUDAPlace(0)
            if core.is_bfloat16_supported(place):
                self.check_output_with_place(place, check_pir=True)

    def test_check_grad_normal(self):
        if core.is_compiled_with_cuda():
            place = core.CUDAPlace(0)
            if core.is_bfloat16_supported(place):
                self.check_grad_with_place(
                    place, ['X', 'Y'], 'Out', check_pir=True
                )
# 测试动静图的版本 


class TestCrossAPI(unittest.TestCase):
    def input_data(self):
        self.data_x = np.array(
            [[1.0, 1.0, 1.0], [2.0, 2.0, 2.0], [3.0, 3.0, 3.0]]
        ).astype('float32')
        self.data_y = np.array(
            [[1.0, 1.0, 1.0], [1.0, 1.0, 1.0], [1.0, 1.0, 1.0]]
        ).astype('float32')

    @test_with_pir_api
    def test_cross_api(self):
        self.input_data()

        main = paddle.static.Program()
        startup = paddle.static.Program()
        # case 1:
        with paddle.static.program_guard(main, startup):
            x = paddle.static.data(name='x', shape=[3, 3], dtype="float32")
            y = paddle.static.data(name='y', shape=[3, 3], dtype="float32")
            z = paddle.cross(x, y, axis=1)
            exe = base.Executor(base.CPUPlace())
            (res,) = exe.run(
                main,
                feed={'x': self.data_x, 'y': self.data_y},
                fetch_list=[z],
                return_numpy=False,
            )
        expect_out = np.array(
            [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
        )
        np.testing.assert_allclose(expect_out, np.array(res), rtol=1e-05)

        main = paddle.static.Program()
        startup = paddle.static.Program()
        # case 2:
        with paddle.static.program_guard(main, startup):
            x = paddle.static.data(name='x', shape=[3, 3], dtype="float32")
            y = paddle.static.data(name='y', shape=[3, 3], dtype="float32")
            z = paddle.cross(x, y)
            exe = base.Executor(base.CPUPlace())
            (res,) = exe.run(
                main,
                feed={'x': self.data_x, 'y': self.data_y},
                fetch_list=[z],
                return_numpy=False,
            )
        expect_out = np.array(
            [[-1.0, -1.0, -1.0], [2.0, 2.0, 2.0], [-1.0, -1.0, -1.0]]
        )
        np.testing.assert_allclose(expect_out, np.array(res), rtol=1e-05)

    def test_cross_api1(self):
        self.input_data()

        main = paddle.static.Program()
        startup = paddle.static.Program()

        # case 1:
        with paddle.static.program_guard(main, startup):
            x = paddle.static.data(name="x", shape=[3, 3], dtype="float32")
            y = paddle.static.data(name='y', shape=[3, 3], dtype='float32')

            y_1 = paddle.cross(x, y, name='result')
            self.assertEqual(('result' in y_1.name), True)

    def test_dygraph_api(self):
        self.input_data()
        # case 1:
        # with base.dygraph.guard():
        #     x = paddle.to_tensor(self.data_x)
        #     y = paddle.to_tensor(self.data_y)
        #     z = paddle.cross(x, y)
        #     np_z = z.numpy()
        # expect_out = np.array([[-1.0, -1.0, -1.0], [2.0, 2.0, 2.0],
        #                        [-1.0, -1.0, -1.0]])
        # np.testing.assert_allclose(expect_out, np_z, rtol=1e-05)

        # case 2:
        with base.dygraph.guard():
            x = paddle.to_tensor(self.data_x)
            y = paddle.to_tensor(self.data_y)
            z = paddle.cross(x, y, axis=1)
            np_z = z.numpy()
        expect_out = np.array(
            [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
        )
        np.testing.assert_allclose(expect_out, np_z, rtol=1e-05)

class TestCrossAPIComplex(unittest.TestCase):
    def set_status(self):
        self.dtype = 'complex64'
        self.part_dtype = 'float32'
        self.shape = [3, 3]
        self.place = base.CPUPlace()
        
    def input_data(self):
        self.set_status()
        self.data_x = (np.random.randint(0, 10, size=(self.shape)).astype(self.part_dtype) + 1j * np.random.randint(0, 10, size=(self.shape)).astype(self.part_dtype)).astype(self.dtype)
        
        self.data_y = (np.random.randint(0, 10, size=(self.shape)).astype(self.part_dtype) + 1j * np.random.randint(0, 10, size=(self.shape)).astype(self.part_dtype)).astype(self.dtype)
        

    @test_with_pir_api
    def test_cross_api(self):
        paddle.enable_static()
        self.input_data()
        main = paddle.static.Program()
        startup = paddle.static.Program()
        # case 1:
        with paddle.static.program_guard(main, startup):
            x = paddle.static.data(name='x', shape=self.shape, dtype=self.dtype)
            y = paddle.static.data(name='y', shape=self.shape, dtype=self.dtype)
            z = paddle.cross(x, y, axis=1)
            exe = base.Executor(self.place)
            (res,) = exe.run(
                main,
                feed={'x': self.data_x, 'y': self.data_y},
                fetch_list=[z],
                return_numpy=False,
            )
        # expect_out = np.array([[ 0.+0.j,  0.+0.j,  0.+0.j],
        # [ 2.+2.j, -4.-4.j,  2.+2.j],
        # [ 6.+6.j, -3.-3.j, -3.-3.j]], dtype=self.dtype)
        expect_out = np.cross(self.data_x, self.data_y, axis=1).astype(self.dtype)
        
        np.testing.assert_allclose(expect_out, np.array(res), rtol=1e-05)

        main = paddle.static.Program()
        startup = paddle.static.Program()
        # case 2:
        with paddle.static.program_guard(main, startup):
            x = paddle.static.data(name='x', shape=self.shape, dtype=self.dtype)
            y = paddle.static.data(name='y', shape=self.shape, dtype=self.dtype)
            z = paddle.cross(x, y, axis=-2)
            exe = base.Executor(self.place)
            (res,) = exe.run(
                main,
                feed={'x': self.data_x, 'y': self.data_y},
                fetch_list=[z],
                return_numpy=False,
            )
        # expect_out = np.array(  
        #     [[ 2.+0.j, -3.-5.j, -2.-4.j],  
        #     [-1.+3.j,  0.+4.j, -2.+2.j],  
        #     [ 0.-2.j,  1.-1.j,  2.+0.j]], dtype=self.dtype  
        # )
        expect_out = np.cross(self.data_x, self.data_y, axis=-2).astype(self.dtype)
        np.testing.assert_allclose(expect_out, np.array(res), rtol=1e-05)

    def test_cross_api1(self):
        self.input_data()
        paddle.enable_static()
        main = paddle.static.Program()
        startup = paddle.static.Program()

        # case 1:
        with paddle.static.program_guard(main, startup):
            x = paddle.static.data(name="x", shape=[3, 3], dtype=self.dtype)
            y = paddle.static.data(name='y', shape=[3, 3], dtype=self.dtype)

            y_1 = paddle.cross(x, y, name='result')
            self.assertEqual(('result' in y_1.name), True)

    def test_dygraph_api(self):
        self.input_data()
        # case 1:
        with base.dygraph.guard(self.place):
            x = paddle.to_tensor(self.data_x)
            y = paddle.to_tensor(self.data_y)
            z = paddle.cross(x, y, axis=-2)
            np_z = z.numpy()
        # expect_out = np.array(  
        #     [[ 2.+0.j, -3.-5.j, -2.-4.j],  
        #     [-1.+3.j,  0.+4.j, -2.+2.j],  
        #     [ 0.-2.j,  1.-1.j,  2.+0.j]], dtype=self.dtype  
        # )
        expect_out = np.cross(self.data_x, self.data_y, axis=-2).astype(self.dtype)
        np.testing.assert_allclose(expect_out, np_z, rtol=1e-05)

        # case 2:
        with base.dygraph.guard(self.place):
            x = paddle.to_tensor(self.data_x)
            y = paddle.to_tensor(self.data_y)
            z = paddle.cross(x, y, axis=1)
            np_z = z.numpy()
        # expect_out = np.array([[ 0.+0.j,  0.+0.j,  0.+0.j],
        # [ 2.+2.j, -4.-4.j,  2.+2.j],
        # [ 6.+6.j, -3.-3.j, -3.-3.j]], dtype=self.dtype)
        expect_out = np.cross(self.data_x, self.data_y, axis=1).astype(self.dtype)
        np.testing.assert_allclose(expect_out, np_z, rtol=1e-05)

class TestCrossAPIComplex128(TestCrossAPIComplex):
    def set_type(self):
        self.dtype = 'complex128'
        self.place = base.CPUPlace()

class TestCrossAPIComplex64Gpu(TestCrossAPIComplex):
    def set_type(self):
        self.dtype = 'complex64'
        self.place = base.GPUPlace()

class TestCrossAPIComplex128Gpu(TestCrossAPIComplex):
    def set_type(self):
        self.dtype = 'complex128'
        self.place = base.GPUPlace()

if __name__ == '__main__':
    paddle.enable_static()
    unittest.main()
