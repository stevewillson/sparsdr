/* -*- c++ -*- */
/*
 * Copyright 2021 The Regents of the University of California.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compressing_pluto_source_impl.h"
#include <gnuradio/io_signature.h>

#include <gnuradio/sparsdr/iio_device_source.h>
#include <boost/lexical_cast.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace gr {
namespace sparsdr {

namespace {

constexpr std::uint32_t DEFAULT_FFT_SIZE = 1024;

bool is_power_of_two(std::uint32_t value) { return (value & (value - 1)) == 0; }
/**
 * Calculates the base-2 logarithm of an integer, assuming that the
 * integer is a power of two
 */
std::uint32_t int_log2(std::uint32_t value)
{
    // Shift right so that int_log2(1) == 0
    value >>= 1;

    std::uint32_t log = 0;
    while (value != 0) {
        log += 1;
        value >>= 1;
    }
    return log;
}
/**
 * Calculates the base-2 logarithm of an integer, rounded up
 */
std::uint32_t ceiling_log2(std::uint32_t value)
{
    if (is_power_of_two(value)) {
        return int_log2(value);
    } else {
        return 1 + int_log2(value);
    }
}
} // namespace

compressing_pluto_source::sptr compressing_pluto_source::make(const std::string& uri,
                                                              std::size_t buffer_size)
{
    return gnuradio::get_initial_sptr(
        new compressing_pluto_source_impl(uri, buffer_size));
}

/*
 * The private constructor
 */
compressing_pluto_source_impl::compressing_pluto_source_impl(const std::string& uri,
                                                             std::size_t buffer_size)
    : gr::hier_block2("compressing_pluto_source",
                      gr::io_signature::make(0, 0, 0),
                      gr::io_signature::make(1, 1, sizeof(uint32_t))),
      d_iio_context(nullptr),
      d_sparsdr_device(nullptr),
      d_ad9361_phy(nullptr),
      d_ad9361_voltage0_in(nullptr),
      d_ad9361_voltage0_out(nullptr),
      d_ad9361_altvoltage0_out(nullptr),
      d_format_version(0),
      d_fft_size(DEFAULT_FFT_SIZE)
{
    d_iio_context = iio_create_context_from_uri(uri.c_str());
    if (!d_iio_context) {
        throw std::runtime_error("Can't create IIO context");
    }

    // Check the format version number
    const char* const format_version_string =
        iio_context_get_attr_value(d_iio_context, "sparsdr_format_version");
    if (format_version_string) {
        d_format_version = boost::lexical_cast<unsigned int>(
            format_version_string, std::strlen(format_version_string));
    } else {
        std::cerr << "IIO context does not have a sparsdr_format_version attribute. "
                  << "Check that the correct SparSDR image is loaded.\n";
        throw std::runtime_error("No sparsdr_format_version attribute");
    }
    // Don't need to free format_version_string

    // Find the SparSDR device and configure it
    d_sparsdr_device = iio_context_find_device(d_iio_context, "sparsdr");
    // TODO: Make logging consistent with GNU Radio conventions
    if (!d_sparsdr_device) {
        std::cerr << "SparSDR device not found on the Pluto radio\n";
        throw std::runtime_error("No SparSDR device");
    }

    iio_device* const cf_ad9361_lpc =
        iio_context_find_device(d_iio_context, "cf-ad9361-lpc");
    if (cf_ad9361_lpc == nullptr) {
        throw std::runtime_error("No cf-ad9361-lpc device found");
    }

    d_ad9361_phy = iio_context_find_device(d_iio_context, "ad9361-phy");
    if (d_ad9361_phy == nullptr) {
        throw std::runtime_error("No ad9361-phy device found");
    }
    d_ad9361_voltage0_in = iio_device_find_channel(d_ad9361_phy, "voltage0", false);
    if (d_ad9361_voltage0_in == nullptr) {
        throw std::runtime_error("Can't find voltage0 input channel on ad9361-phy");
    }
    d_ad9361_voltage0_out = iio_device_find_channel(d_ad9361_phy, "voltage0", true);
    if (d_ad9361_voltage0_out == nullptr) {
        throw std::runtime_error("Can't find voltage0 output channel on ad9361-phy");
    }
    d_ad9361_altvoltage0_out = iio_device_find_channel(d_ad9361_phy, "altvoltage0", true);
    if (d_ad9361_altvoltage0_out == nullptr) {
        throw std::runtime_error("Can't find altvoltage0 channel on ad9361-phy");
    }
    // Basic required configuration
    configure_ad9361_phy();

    // Create IIO device source block and connect
    // The device source will not destroy the IIO context.
    const auto source_block =
        iio_device_source::make(cf_ad9361_lpc, "voltage0", buffer_size);
    connect(source_block, 0, self(), 0);
}

unsigned int compressing_pluto_source_impl::format_version() const
{
    return d_format_version;
}

void compressing_pluto_source_impl::set_frequency(unsigned long long frequency)
{
    iio_channel* const altvoltage0 =
        iio_device_find_channel(d_ad9361_phy, "altvoltage0", true);
    if (altvoltage0 == nullptr) {
        throw std::runtime_error("Can't find altvoltage0 output channel on ad9361-phy");
    }
    const std::string frequency_string = std::to_string(frequency);
    const ssize_t status =
        iio_channel_attr_write(altvoltage0, "frequency", frequency_string.c_str());
    if (status < 0) {
        throw std::runtime_error("Failed to write frequency attribute");
    }
}
void compressing_pluto_source_impl::set_gain(double gain)
{
    iio_channel* const in_voltage0 =
        iio_device_find_channel(d_ad9361_phy, "voltage0", false);
    if (in_voltage0 == nullptr) {
        throw std::runtime_error("Can't find voltage0 input channel on ad9361-phy");
    }
    const std::string gain_string = std::to_string(gain);
    const ssize_t status =
        iio_channel_attr_write(in_voltage0, "hardwaregain", gain_string.c_str());
    if (status < 0) {
        throw std::runtime_error("Failed to write gain attribute");
    }
}


void compressing_pluto_source_impl::set_gain_control_mode(const std::string& mode)
{
    iio_channel* const in_voltage0 =
        iio_device_find_channel(d_ad9361_phy, "voltage0", false);
    if (in_voltage0 == nullptr) {
        throw std::runtime_error("Can't find voltage0 input channel on ad9361-phy");
    }
    const ssize_t status =
        iio_channel_attr_write(in_voltage0, "gain_control_mode", mode.c_str());
    if (status < 0) {
        throw std::runtime_error("Failed to write gain_control_mode attribute");
    }
}

void compressing_pluto_source_impl::set_compression_enabled(bool enabled)
{
    write_bool_attr("enable_compression", enabled);
}

void compressing_pluto_source_impl::set_run_fft(bool enable)
{
    write_bool_attr("run_fft", enable);
}
void compressing_pluto_source_impl::set_send_average_samples(bool enable)
{
    write_bool_attr("send_average_samples", enable);
}
void compressing_pluto_source_impl::set_send_fft_samples(bool enable)
{
    write_bool_attr("send_fft_samples", enable);
}
void compressing_pluto_source_impl::set_fft_size(std::uint32_t size)
{
    if (!is_power_of_two(size) || size < 8 || size > 1024) {
        throw std::invalid_argument(
            "FFT size must be a power of two between 8 and 1024 inclusive");
    }
    // Register value is the base-2 logarithm of the FFT size
    const unsigned int log_size = int_log2(size);
    write_u32_attr("fft_size", log_size);
    d_fft_size = size;
}
std::uint32_t compressing_pluto_source_impl::fft_size() const { return d_fft_size; }
void compressing_pluto_source_impl::set_shift_amount(std::uint8_t amount)
{
    if (amount > 8) {
        throw std::invalid_argument("Shift amount must be in the range [0, 8]");
    }
    write_u32_attr("shift_amount", amount);
}
void compressing_pluto_source_impl::set_bin_threshold(std::uint16_t bin_index,
                                                      std::uint32_t threshold)
{
    // The threshold value is latched when the bin number is written
    write_u32_attr("threshold_value", threshold);
    write_u32_attr("threshold_bin_number", bin_index);
}
void compressing_pluto_source_impl::set_bin_window_value(std::uint16_t bin_index,
                                                         std::uint16_t value)
{
    write_u32_attr("window_value",
                   (std::uint32_t(bin_index) << 16) | std::uint32_t(value));
}

void compressing_pluto_source_impl::set_bin_mask(std::uint16_t bin_index)
{
    write_u32_attr("bin_mask", (std::uint32_t(bin_index) << 1) | 0x1);
}
void compressing_pluto_source_impl::clear_bin_mask(std::uint16_t bin_index)
{
    write_u32_attr("bin_mask", std::uint32_t(bin_index) << 1 | 0x0);
}

void compressing_pluto_source_impl::set_average_weight(float weight)
{
    if (weight < 0.0 || weight >= 1.0 || std::isnan(weight)) {
        throw std::invalid_argument(
            "Average weight must be greater than or equal to 0 and less than 1");
    }
    // Map from [0, 1) to [0, 256)
    const std::uint32_t register_value = uint32_t(weight * 256.0);
    write_u32_attr("average_weight", register_value);
}
void compressing_pluto_source_impl::set_average_interval(std::uint32_t interval)
{
    if (interval < 8 || interval > 2147483648) {
        throw std::invalid_argument(
            "Average interval must be between 8 and 2147483648 inclusive");
    }
    // Actual register value is the base-2 logarithm of the interval
    write_u32_attr("average_interval", ceiling_log2(interval));
}

void compressing_pluto_source_impl::write_bool_attr(const char* name, bool value)
{
    const char* value_text = value ? "1" : "0";
    const ssize_t status = iio_device_attr_write(d_sparsdr_device, name, value_text);
    // Expected return value includes one extra byte
    if (status != 2) {
        throw std::runtime_error("Failed to write boolean attribute");
    }
}

void compressing_pluto_source_impl::write_u32_attr(const char* name, std::uint32_t value)
{
    const std::string string_value = std::to_string(value);
    const ssize_t status =
        iio_device_attr_write(d_sparsdr_device, name, string_value.c_str());
    if (status < 0) {
        throw std::runtime_error("Failed to write u32 attribute");
    }
}

/**
 * Configures the sampling frequency, bandwidth, and gain control mode on an
 * ad9361-phy device to work with SparSDR
 */
void compressing_pluto_source_impl::configure_ad9361_phy()
{
    const ssize_t sampling_frequency_status = iio_channel_attr_write_longlong(
        d_ad9361_voltage0_out, "sampling_frequency", 61440000);
    if (sampling_frequency_status < 0) {
        throw std::runtime_error("Failed to write voltage0 output sampling_frequency");
    }
    const ssize_t bandwidth_status =
        iio_channel_attr_write_longlong(d_ad9361_voltage0_in, "rf_bandwidth", 56000000);
    if (bandwidth_status < 0) {
        throw std::runtime_error("Failed to write rf_bandwidth");
    }
    const ssize_t gain_control_status =
        iio_channel_attr_write(d_ad9361_voltage0_in, "gain_control_mode", "manual");
    if (gain_control_status < 0) {
        throw std::runtime_error("Failed to write gain_control_mode");
    }
}

/*
 * Our virtual destructor.
 */
compressing_pluto_source_impl::~compressing_pluto_source_impl()
{
    iio_context_destroy(d_iio_context);
}


} /* namespace sparsdr */
} /* namespace gr */
