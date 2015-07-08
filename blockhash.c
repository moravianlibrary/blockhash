#include "blockhash.h"

#include <vector>
#include <algorithm>
#include <cmath>

#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;
using namespace cv;

inline bool is_power_of_two(int x) {
    return !(x == 0) && !(x & (x - 1));
}

inline int total_value(Mat& image, int cx, int cy) {
    Vec3b intensity = image.at<Vec3b>(cy, cx);
    return intensity.val[0] + intensity.val[1] + intensity.val[2];
}

template<typename T>
double median(const typename vector<T>::const_iterator& begin, const typename vector<T>::const_iterator& end) {
    vector<T> data(begin, end);
    sort(data.begin(), data.end());
    int length = data.size();
    
    if (length % 2 == 0) {
        return (data[length / 2] + data[length / 2 + 1]) / 2.0;
    } else {
        return data[length / 2];
    }
}

template<typename T>
vector<uint64_t> convert(const vector<T>& data) {
    vector<uint64_t> result;
    
    for (typename vector<T>::const_iterator it = data.begin(); it != data.end();) {
        uint64_t item = 0;
        for (int i = 0; i < 64 && it != data.end(); i++, it++) {
            if (*it != (T) 0) {
                item |= (uint64_t) 1 << (63 - i);
            }
        }
        result.push_back(item);
    }
    return result;
}

template<typename T>
void attach_bits(
        const typename vector<T>::iterator& blocks_start,
        const typename vector<T>::iterator& blocks_end,
        double median) {
    
    for (typename vector<T>::iterator it = blocks_start; it != blocks_end; it++) {
        if (*it < median) {
            *it = 0;
        } else {
            *it = 1;
        }
    }
}

template<typename T>
vector<uint64_t> compute_median_and_attach_bits(vector<T>& blocks, int blocksize) {
    int step = blocksize * blocksize / 4;
    typename vector<T>::iterator blocks_it = blocks.begin();
    for (int i = 0; i < 4; i++) {
        double m = median<T>(blocks_it, blocks_it + step);
        attach_bits<T>(blocks_it, blocks_it + step, m);
        blocks_it += step;
    }
    return convert<T>(blocks);
}

vector<uint64_t> compute_internal_even(Mat& image, int blocksize) {
    int width = image.cols;
    int height = image.rows;
    int blockwidth = width / blocksize;
    int blockheight = height / blocksize;
    
    vector<int> blocks;
    blocks.reserve(blocksize * blocksize);
    
    for (int y = 0; y < blocksize; y++) {
        for (int x = 0; x < blocksize; x++) {
            int value = 0;
            
            for (int iy = 0; iy < blockheight; iy++) {
                for (int ix = 0; ix < blockwidth; ix++) {
                    int cx = x * blockwidth + ix;
                    int cy = y * blockheight + iy;
                    value += total_value(image, cx, cy);
                }
            }
            blocks.push_back(value);
        }
    }
    
    return compute_median_and_attach_bits<int>(blocks, blocksize);
}

vector<uint64_t> compute_internal_odd(Mat& image, int blocksize) {
    int width = image.cols;
    int height = image.rows;
    float block_width = (float) width / (float) blocksize;
    float block_height = (float) height / (float) blocksize;
    float y_frac, y_int;
    float x_frac, x_int;
    float x_mod, y_mod;
    float weight_top, weight_bottom, weight_left, weight_right;
    int block_top, block_bottom, block_left, block_right;
    
    vector<double> blocks(blocksize * blocksize, 0);
    
    for (int y = 0; y < height; y++) {
        y_mod = fmodf(y + 1, block_height);
        y_frac = modff(y_mod, &y_int);

        weight_top = (1 - y_frac);
        weight_bottom = y_frac;

        // y_int will be 0 on bottom/right borders and on block boundaries
        if (y_int > 0 || (y + 1) == height) {
            block_top = block_bottom = (int) floor((float) y / block_height);
        } else {
            block_top = (int) floor((float) y / block_height);
            block_bottom = (int) ceil((float) y / block_height);
        }
        
        for (int x = 0; x < width; x++) {
            x_mod = fmodf(x + 1, block_width);
            x_frac = modff(x_mod, &x_int);

            weight_left = (1 - x_frac);
            weight_right = x_frac;

            // x_int will be 0 on bottom/right borders and on block boundaries
            if (x_int > 0 || (x + 1) == width) {
                block_left = block_right = (int) floor((float) x / block_width);
            } else {
                block_left = (int) floor((float) x / block_width);
                block_right = (int) ceil((float) x / block_width);
            }
            
            double value = total_value(image, x, y);
            // add weighted pixel value to relevant blocks
            blocks[block_top * blocksize + block_left] += value * weight_top * weight_left;
            blocks[block_top * blocksize + block_right] += value * weight_top * weight_right;
            blocks[block_bottom * blocksize + block_left] += value * weight_bottom * weight_left;
            blocks[block_bottom * blocksize + block_right] += value * weight_bottom * weight_right;
        }
    }

    return compute_median_and_attach_bits<double>(blocks, blocksize);
}

vector<uint64_t> compute_internal(Mat& image, int blocksize) {
    int width = image.cols;
    int height = image.rows;
    
    bool even_x = (width % blocksize) == 0;
    bool even_y = (height % blocksize) == 0;
    
    if (even_x && even_y) {
        return compute_internal_even(image, blocksize);
    } else {
        return compute_internal_odd(image, blocksize);
    }
}

vector<uint64_t> blockhash::compute(const std::string& filename, int size) {
    if (!is_power_of_two(size)) {
        throw invalid_argument("Size must be power of 2.");
    }
    
    Mat image = imread(filename, CV_LOAD_IMAGE_COLOR);
    return compute_internal(image, sqrt(size));
}
    
vector<uint64_t> blockhash::compute(const std::vector<uint8_t>& data, int size) {
    if (!is_power_of_two(size)) {
        throw invalid_argument("Size must be power of 2.");
    }
    
    Mat image = imdecode(data, CV_LOAD_IMAGE_COLOR);
    return compute_internal(image, sqrt(size));
}
