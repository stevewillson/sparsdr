/* -*- c++ -*- */
/*
 * Copyright 2019 The Regents of the University of California
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


#ifndef INCLUDED_SPARSDR_BAND_SPEC_VECTOR_H
#define INCLUDED_SPARSDR_BAND_SPEC_VECTOR_H
#include <cstdint>
#include <vector>
#include <gnuradio/sparsdr/api.h>
#include "band_spec.h"

namespace gr {
namespace sparsdr {
/*!
 * \brief A specification of a vector of bands to reconstruct
 */
class GR_SPARSDR_API band_spec_vector
{

private:
    std::vector<::gr::sparsdr::band_spec> bsv;

public:
    /*!
     * \brief create a vector of band specifications
     * 
     */
    inline band_spec_vector() : bsv(0)
    {}

    inline void push_back(::gr::sparsdr::band_spec elem)
    {
        bsv.push_back(elem);
    }

    inline std::vector<::gr::sparsdr::band_spec>& get_values()
    {
        return bsv;
    }
};

} // namespace sparsdr
} // namespace gr

#endif