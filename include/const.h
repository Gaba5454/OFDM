// const.h
#ifndef CONST_H
#define CONST_H
#include <iostream>
#include <fftw3.h>
#include <math.h>
#include <complex>
#include <vector>
#include <bitset>
#include <fstream>
#include <random> 
#include <cstdint>

static const std::complex<double> j(0.0, 1.0);

constexpr uint8_t LTE = 128;
const size_t CP_LENGTH = 20;  // Length of the CP ~1/12 on 128 symbols
using CD = std::complex<double>;
#endif