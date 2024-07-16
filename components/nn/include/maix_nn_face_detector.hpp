/**
 * @author neucrack@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2024.5.15: Create this file.
 */

#pragma once
#include "maix_basic.hpp"
#include "maix_nn.hpp"
#include "maix_image.hpp"
#include "maix_nn_F.hpp"
#include "maix_nn_object.hpp"
#include <tuple>

namespace maix::nn
{
    /**
     * FaceDetector class
     * @maixpy maix.nn.FaceDetector
     */
    class FaceDetector
    {
    public:
    public:
        /**
         * Constructor of FaceDetector class
         * @param model model path, default empty, you can load model later by load function.
         * @param[in] dual_buff prepare dual input output buffer to accelarate forward, that is, when NPU is forwarding we not wait and prepare the next input buff.
         *                      If you want to ensure every time forward output the input's result, set this arg to false please.
         *                      Default true to ensure speed.
         * @throw If model arg is not empty and load failed, will throw err::Exception.
         * @maixpy maix.nn.FaceDetector.__init__
         * @maixcdk maix.nn.FaceDetector.FaceDetector
         */
        FaceDetector(const string &model = "", bool dual_buff = true)
        {
            _model = nullptr;
            _dual_buff = dual_buff;
            if (!model.empty())
            {
                err::Err e = load(model);
                if (e != err::ERR_NONE)
                {
                    throw err::Exception(e, "load model failed");
                }
            }
        }

        ~FaceDetector()
        {
            if (_model)
            {
                delete _model;
                _model = nullptr;
            }
        }

        /**
         * Load model from file
         * @param model Model path want to load
         * @return err::Err
         * @maixpy maix.nn.FaceDetector.load
         */
        err::Err load(const string &model)
        {
            if (_model)
            {
                delete _model;
                _model = nullptr;
            }
            _model = new nn::NN(model, _dual_buff);
            if (!_model)
            {
                return err::ERR_NO_MEM;
            }
            _extra_info = _model->extra_info();
            if (_extra_info.find("model_type") != _extra_info.end())
            {
                if (_extra_info["model_type"] != "face_detector")
                {
                    log::error("model_type not match, expect 'face_detector', but got '%s'", _extra_info["model_type"].c_str());
                    return err::ERR_ARGS;
                }
            }
            else
            {
                log::error("model_type key not found");
                return err::ERR_ARGS;
            }
            log::info("model info:\n\ttype: face_detector");
            if (_extra_info.find("input_type") != _extra_info.end())
            {
                std::string input_type = _extra_info["input_type"];
                if (input_type == "rgb")
                {
                    _input_img_fmt = maix::image::FMT_RGB888;
                    log::print("\tinput type: rgb\n");
                }
                else if (input_type == "bgr")
                {
                    _input_img_fmt = maix::image::FMT_BGR888;
                    log::print("\tinput type: bgr\n");
                }
                else
                {
                    log::error("unknown input type: %s", input_type.c_str());
                    return err::ERR_ARGS;
                }
            }
            else
            {
                log::error("input_type key not found");
                return err::ERR_ARGS;
            }
            if (_extra_info.find("mean") != _extra_info.end())
            {
                std::string mean_str = _extra_info["mean"];
                std::vector<std::string> mean_strs = split(mean_str, ",");
                log::print("\tmean:");
                for (auto &it : mean_strs)
                {
                    try
                    {
                        this->mean.push_back(std::stof(it));
                    }
                    catch (std::exception &e)
                    {
                        log::error("mean value error, should float");
                        return err::ERR_ARGS;
                    }
                    log::print("%f ", this->mean.back());
                }
                log::print("\n");
            }
            else
            {
                log::error("mean key not found");
                return err::ERR_ARGS;
            }
            if (_extra_info.find("scale") != _extra_info.end())
            {
                std::string scale_str = _extra_info["scale"];
                std::vector<std::string> scale_strs = split(scale_str, ",");
                log::print("\tscale:");
                for (auto &it : scale_strs)
                {
                    try
                    {
                        this->scale.push_back(std::stof(it));
                    }
                    catch (std::exception &e)
                    {
                        log::error("scale value error, should float");
                        return err::ERR_ARGS;
                    }
                    log::print("%f ", this->scale.back());
                }
                log::print("\n");
            }
            else
            {
                log::error("scale key not found");
                return err::ERR_ARGS;
            }
            std::vector<nn::LayerInfo> inputs = _model->inputs_info();
            _input_size = image::Size(inputs[0].shape[3], inputs[0].shape[2]);
            log::print("\tinput size: %dx%d\n\n", _input_size.width(), _input_size.height());

            // decoder params
            // priorbox
            std::vector<std::tuple<int, int>> feature_maps;
            int steps[] = {8, 16, 32, 64};
            _variance.push_back(0.1);
            _variance.push_back(0.2);
            std::vector<std::vector<int>> min_sizes = {
                {10, 16, 24},
                {32, 48},
                {64, 96},
                {128, 192, 256}
            };
            for(size_t i=0; i<sizeof(steps) / sizeof(int); ++i)
            {
                feature_maps.push_back(std::tuple<int, int>(ceil(_input_size.height() / steps[i]), ceil(_input_size.width() / steps[i])));
            }
            for(size_t i=0; i<feature_maps.size(); ++i)
            {
                for(int j=0; j < std::get<0>(feature_maps[i]); ++j)
                {
                    for(int k=0; k < std::get<1>(feature_maps[i]); ++k)
                    {
                        for(size_t m=0; m < min_sizes[i].size(); ++m)
                        {
                            float s_kx = min_sizes[i][m] * 1.0 / _input_size.width();
                            float s_ky = min_sizes[i][m] * 1.0 / _input_size.height();
                            float dense_cx = (k + 0.5) * steps[i] / _input_size.width();
                            float dense_cy = (j + 0.5) * steps[i] / _input_size.height();
                            _anchor.push_back({dense_cx, dense_cy, s_kx, s_ky});
                        }
                    }
                }
            }

            return err::ERR_NONE;
        }

        /**
         * Detect objects from image
         * @param img Image want to detect, if image's size not match model input's, will auto resize with fit method.
         * @param conf_th Confidence threshold, default 0.5.
         * @param iou_th IoU threshold, default 0.45.
         * @param fit Resize method, default image.Fit.FIT_CONTAIN.
         * @throw If image format not match model input format, will throw err::Exception.
         * @return Object list. In C++, you should delete it after use.
         * @maixpy maix.nn.FaceDetector.detect
         */
        std::vector<nn::Object> *detect(image::Image &img, float conf_th = 0.5, float iou_th = 0.45, maix::image::Fit fit = maix::image::FIT_CONTAIN)
        {
            this->_conf_th = conf_th;
            this->_iou_th = iou_th;
            if (img.format() != _input_img_fmt)
            {
                throw err::Exception("image format not match, input_type: " + image::fmt_names[_input_img_fmt] + ", image format: " + image::fmt_names[img.format()]);
            }
            tensor::Tensors *outputs;
            outputs = _model->forward_image(img, this->mean, this->scale, fit, false);
            if (!outputs)
            {
                return new std::vector<nn::Object>();
            }
            std::vector<nn::Object> *res = _post_process(outputs, img.width(), img.height(), fit);
            delete outputs;
            return res;
        }

        /**
         * Get model input size
         * @return model input size
         * @maixpy maix.nn.FaceDetector.input_size
         */
        image::Size input_size()
        {
            return _input_size;
        }

        /**
         * Get model input width
         * @return model input size of width
         * @maixpy maix.nn.FaceDetector.input_width
         */
        int input_width()
        {
            return _input_size.width();
        }

        /**
         * Get model input height
         * @return model input size of height
         * @maixpy maix.nn.FaceDetector.input_height
         */
        int input_height()
        {
            return _input_size.height();
        }

        /**
         * Get input image format
         * @return input image format, image::Format type.
         * @maixpy maix.nn.FaceDetector.input_format
         */
        image::Format input_format()
        {
            return _input_img_fmt;
        }

    public:
        /**
         * Get mean value, list type
         * @maixpy maix.nn.FaceDetector.mean
         */
        std::vector<float> mean;

        /**
         * Get scale value, list type
         * @maixpy maix.nn.FaceDetector.scale
         */
        std::vector<float> scale;

    private:
        image::Size _input_size;
        image::Format _input_img_fmt;
        nn::NN *_model;
        std::map<string, string> _extra_info;
        float _conf_th = 0.5;
        float _iou_th = 0.45;
        bool _dual_buff;
        std::vector<std::vector<float>> _anchor; // [[dense_cx, dense_cy, s_kx, s_ky],]
        std::vector<float> _variance;

    private:
        std::vector<nn::Object> *_post_process(tensor::Tensors *outputs, int img_w, int img_h, maix::image::Fit fit)
        {
            std::vector<nn::Object> *objects = new std::vector<nn::Object>();
            tensor::Tensor *conf = nullptr;
            tensor::Tensor *loc = nullptr;
            tensor::Tensor *landms = nullptr;
            for(auto i : outputs->tensors)
            {
                if(i.second->shape()[2] == 2)
                    conf = i.second;
                else if(i.second->shape()[2] == 4)
                    loc = i.second;
                else if(i.second->shape()[2] == 10)
                    landms = i.second;
            }
            if(!conf || !loc || !landms)
                return nullptr;
            float *conf_data = (float *)conf->data();
            float *loc_data = (float *)loc->data();
            float *landms_data = (float *)landms->data();

            std::vector<int> valid_idx;
            for (int i = 0; i < conf->size_int() / 2; ++i)
            {
                if (conf_data[i * 2 + 1] >= this->_conf_th)
                {
                    valid_idx.push_back(i);
                }
            }
            for (int idx : valid_idx)
            {
                float x = (_anchor[idx][0] + loc_data[idx * 4] * _variance[0] * _anchor[idx][2]) * _input_size.width();
                float y = (_anchor[idx][1] + loc_data[idx * 4 + 1] * _variance[0] * _anchor[idx][3]) * _input_size.height();
                float w = _anchor[idx][2] * exp(loc_data[idx * 4 + 2] * _variance[1]) * _input_size.width();
                float h = _anchor[idx][3] * exp(loc_data[idx * 4 + 3] * _variance[1]) * _input_size.height();
                std::vector<int> points;
                points.push_back(_input_size.width() * (_anchor[idx][0] + landms_data[idx * 10] * _variance[0] * _anchor[idx][2]));
                points.push_back(_input_size.height() * (_anchor[idx][1] + landms_data[idx * 10 + 1] * _variance[0] * _anchor[idx][3]));
                points.push_back(_input_size.width() * (_anchor[idx][0] + landms_data[idx * 10 + 2] * _variance[0] * _anchor[idx][2]));
                points.push_back(_input_size.height() * (_anchor[idx][1] + landms_data[idx * 10 + 3] * _variance[0] * _anchor[idx][3]));
                points.push_back(_input_size.width() * (_anchor[idx][0] + landms_data[idx * 10 + 4] * _variance[0] * _anchor[idx][2]));
                points.push_back(_input_size.height() * (_anchor[idx][1] + landms_data[idx * 10 + 5] * _variance[0] * _anchor[idx][3]));
                points.push_back(_input_size.width() * (_anchor[idx][0] + landms_data[idx * 10 + 6] * _variance[0] * _anchor[idx][2]));
                points.push_back(_input_size.height() * (_anchor[idx][1] + landms_data[idx * 10 + 7] * _variance[0] * _anchor[idx][3]));
                points.push_back(_input_size.width() * (_anchor[idx][0] + landms_data[idx * 10 + 8] * _variance[0] * _anchor[idx][2]));
                points.push_back(_input_size.height() * (_anchor[idx][1] + landms_data[idx * 10 + 9] * _variance[0] * _anchor[idx][3]));
                nn::Object object((int)(x - w/2), (int)(y - h/2), w, h, 0, conf_data[idx * 2 + 1], points);
                objects->push_back(object);
            }
            if (objects->size() > 0)
            {
                std::vector<nn::Object> *objects_total = objects;
                objects = _nms(*objects);
                delete objects_total;
            }
            if (objects->size() > 0)
                _correct_bbox(*objects, img_w, img_h, fit);
            return objects;
        }

        std::vector<nn::Object> *_nms(std::vector<nn::Object> &objs)
        {
            std::vector<nn::Object> *result = new std::vector<nn::Object>();
            std::sort(objs.begin(), objs.end(), [](const nn::Object &a, const nn::Object &b)
                      { return a.score < b.score; });
            for (size_t i = 0; i < objs.size(); ++i)
            {
                nn::Object &a = objs.at(i);
                if (a.score == 0)
                    continue;
                for (size_t j = i + 1; j < objs.size(); ++j)
                {
                    nn::Object &b = objs.at(j);
                    if (b.score != 0 && a.class_id == b.class_id && _calc_iou(a, b) > this->_iou_th)
                    {
                        b.score = 0;
                    }
                }
            }
            for (nn::Object &a : objs)
            {
                if (a.score != 0)
                    result->push_back(a);
            }
            return result;
        }

        void _correct_bbox(std::vector<nn::Object> &objs, int img_w, int img_h, maix::image::Fit fit)
        {
            if (img_w == _input_size.width() && img_h == _input_size.height())
                return;
            if (fit == maix::image::FIT_FILL)
            {
                float scale_x = (float)img_w / _input_size.width();
                float scale_y = (float)img_h / _input_size.height();
                for (nn::Object &obj : objs)
                {
                    obj.x *= scale_x;
                    obj.y *= scale_y;
                    obj.w *= scale_x;
                    obj.h *= scale_y;
                    for(size_t i=0; i<obj.points.size() / 2; ++i)
                    {
                        obj.points.at(i * 2) *= scale_x;
                        obj.points.at(i * 2 + 1) *= scale_y;
                    }
                }
            }
            else if (fit == maix::image::FIT_CONTAIN)
            {
                float scale_x = ((float)_input_size.width()) / img_w;
                float scale_y = ((float)_input_size.height()) / img_h;
                float scale = std::min(scale_x, scale_y);
                float scale_reverse = 1.0 / scale;
                float pad_w = (_input_size.width() - img_w * scale) / 2.0;
                float pad_h = (_input_size.height() - img_h * scale) / 2.0;
                for (nn::Object &obj : objs)
                {
                    obj.x = (obj.x - pad_w) * scale_reverse;
                    obj.y = (obj.y - pad_h) * scale_reverse;
                    obj.w *= scale_reverse;
                    obj.h *= scale_reverse;
                    for(size_t i=0; i<obj.points.size() / 2; ++i)
                    {
                        obj.points.at(i * 2) = (obj.points.at(i * 2) - pad_w) * scale_reverse;
                        obj.points.at(i * 2 + 1) = (obj.points.at(i * 2 + 1) - pad_h) * scale_reverse;
                    }
                }
            }
            else if (fit == maix::image::FIT_COVER)
            {
                float scale_x = ((float)_input_size.width()) / img_w;
                float scale_y = ((float)_input_size.height()) / img_h;
                float scale = std::max(scale_x, scale_y);
                float scale_reverse = 1.0 / scale;
                float pad_w = (img_w * scale - _input_size.width()) / 2.0;
                float pad_h = (img_h * scale - _input_size.height()) / 2.0;
                for (nn::Object &obj : objs)
                {
                    obj.x = (obj.x + pad_w) * scale_reverse;
                    obj.y = (obj.y + pad_h) * scale_reverse;
                    obj.w *= scale_reverse;
                    obj.h *= scale_reverse;
                    for(size_t i=0; i<obj.points.size() / 2; ++i)
                    {
                        obj.points.at(i * 2) = (obj.points.at(i * 2) - pad_w) * scale_reverse;
                        obj.points.at(i * 2 + 1) = (obj.points.at(i * 2 + 1) - pad_h) * scale_reverse;
                    }
                }
            }
            else
            {
                throw err::Exception(err::ERR_ARGS, "fit type not support");
            }
        }

        inline static float _sigmoid(float x) { return 1.0 / (1 + expf(-x)); }

        inline static float _calc_iou(Object &a, Object &b)
        {
            float area1 = a.w * a.h;
            float area2 = b.w * b.h;
            float wi = std::min((a.x + a.w), (b.x + b.w)) -
                       std::max(a.x, b.x);
            float hi = std::min((a.y + a.h), (b.y + b.h)) -
                       std::max(a.y, b.y);
            float area_i = std::max(wi, 0.0f) * std::max(hi, 0.0f);
            return area_i / (area1 + area2 - area_i);
        }

        template <typename T>
        static int _argmax(const T *data, size_t len, size_t stride = 1)
        {
            int maxIndex = 0;
            for (size_t i = 1; i < len; i++)
            {
                int idx = i * stride;
                if (data[maxIndex * stride] < data[idx])
                {
                    maxIndex = i;
                }
            }
            return maxIndex;
        }

        static void split0(std::vector<std::string> &items, const std::string &s, const std::string &delimiter)
        {
            items.clear();
            size_t pos_start = 0, pos_end, delim_len = delimiter.length();
            std::string token;

            while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
            {
                token = s.substr(pos_start, pos_end - pos_start);
                pos_start = pos_end + delim_len;
                items.push_back(token);
            }

            items.push_back(s.substr(pos_start));
        }

        static std::vector<std::string> split(const std::string &s, const std::string &delimiter)
        {
            std::vector<std::string> tokens;
            split0(tokens, s, delimiter);
            return tokens;
        }
    };

} // namespace maix::nn
