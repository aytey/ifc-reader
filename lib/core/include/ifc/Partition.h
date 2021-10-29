#pragma once

#include "common_types.h"

#include <span>

namespace ifc
{
    struct PartitionSummary
    {
        TextOffset name;
        ByteOffset offset;
        Cardinality cardinality;
        EntitySize entry_size;

        size_t size_bytes() const
        {
            return raw_count(cardinality) * static_cast<size_t>(entry_size);
        }
    };

    template<typename Index>
    Index get_raw_index(Index index)
    {
        return index;
    }

    template<int N, typename Sort>
    uint32_t get_raw_index(AbstractReference<N, Sort> ref)
    {
        return ref.index;
    }

    template<typename T, typename Index = uint32_t>
    class Partition : std::span<T const>
    {
        using Base = std::span<T const>;

    public:
        T const & operator[] (Index index) const
        {
            return Base::operator[](static_cast<size_t>(get_raw_index(index)));
        }

        using Base::begin;
        using Base::end;
        using Base::size;

        Partition slice(Sequence seq)
        {
            return { Base::subspan(static_cast<size_t>(seq.start), raw_count(seq.cardinality)) };
        }

        Partition(std::span<T const> raw_data)
            : Base(raw_data)
        {}
    };
}
