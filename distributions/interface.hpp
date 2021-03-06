/**
 * Copyright (C) 2018 Dean De Leo, email: dleo[at]cwi.nl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DISTRIBUTIONS_INTERFACE_HPP_
#define DISTRIBUTIONS_INTERFACE_HPP_

#include <cinttypes>
#include <cstddef>
#include <memory>
#include <ostream>
#include <utility>

namespace distributions {

// An element to store in the index, seen as a pair <search key, associated value>
using KeyValue = std::pair<int64_t, int64_t>;

/**
 * Common interface for all distributions. An implementation of this interface yields a sequence of elements,
 * as pairs <key, value> according to a specific distribution, e.g. uniform, zipf, sequential.
 */
class Interface {
public:

    /**
     * Destructor
     */
    virtual ~Interface();

    /**
     * Number of elements that can be generated by the underlying instance
     */
    virtual size_t size() const = 0;

    /**
     * Retrieve the pair <key, value> at the given position
     */
    virtual KeyValue get(size_t index) const;

    /**
     * Retrieve only the key at the given position
     */
    virtual int64_t key(size_t index) const = 0;

    /**
     * A sub-sequence of the complete generated sequence, starting from position `shift'
     */
    virtual std::unique_ptr<Interface> view(size_t shift);

    /**
     * A sub-sequence of the complete generated sequence, starting from position `start', having the provided `length' or
     * total number of elements
     */
    virtual std::unique_ptr<Interface> view(size_t start, size_t length) = 0;

    /**
     * Whether the set of generated numbers is a sequence with no gaps and no duplicates, containing
     * all numbers of the interval requested.
     * e.g. [6, 8, 7, 4, 5] is a dense sequence for the interval [5,8].
     */
    virtual bool is_dense() const;
};

/**
 * Print to the output stream the pair <key, value>, for debugging purposes
 */
std::ostream& operator << (std::ostream& out, const KeyValue& k);

} // namespace distributions

#endif /* DISTRIBUTIONS_INTERFACE_HPP_ */
