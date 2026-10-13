// The order the game takes its add-ins and offers in, which decides which
// add-in's copy of a file it uses (dev/FILES.md): the items of AddIns.xml or
// offers.xml, the last in the file first, sorted by MSVC 2005's std::sort on
// their Priority, the highest first. That sort is not stable, so items of
// equal Priority come out in an order of its own; this runs the same sort to
// give the same order.
//
//   const auto order = dao::core::AddInOrder(items, true);
//   // order[0] is visited first: its folders have the highest priority number, and lose
#pragma once

#include "dao/core/AddIns.h"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace dao::core
{

namespace vc8
{

// MSVC 2005's std::sort (its <algorithm>: _Sort, _Unguarded_partition,
// _Median, _Insertion_sort), for a vector, step for step: a quicksort that
// gathers the pivot's equals as it goes, a median of three (of nine past 40
// items), and an insertion sort under 33 items. Heap sort, which it falls
// back on past 1.5 log2(n) divisions, is not reached by the game's few
// hundred add-ins at most and is not here: past that depth this finishes
// with the insertion sort, which gives a sorted order, if maybe not the
// game's.
template <class T, class Less> class Sorter
{
  public:
    Sorter(std::vector<T> &items, Less less) : a_(items), less_(less)
    {
    }

    void Sort()
    {
        Range(0, a_.size(), a_.size());
    }

  private:
    static constexpr std::size_t kInsertionMost = 32;
    static constexpr std::size_t kNintherPast = 40;

    void Swap(std::size_t i, std::size_t j)
    {
        std::swap(a_[i], a_[j]);
    }

    void Median3(std::size_t first, std::size_t mid, std::size_t last)
    {
        if (less_(a_[mid], a_[first]))
        {
            Swap(mid, first);
        }
        if (less_(a_[last], a_[mid]))
        {
            Swap(last, mid);
        }
        if (less_(a_[mid], a_[first]))
        {
            Swap(mid, first);
        }
    }

    void Median(std::size_t first, std::size_t mid, std::size_t last)
    {
        if (kNintherPast < last - first)
        {
            const std::size_t step = (last - first + 1) / 8;
            Median3(first, first + step, first + (2 * step));
            Median3(mid - step, mid, mid + step);
            Median3(last - (2 * step), last - step, last);
            Median3(first + step, mid, last - step);
        }
        else
        {
            Median3(first, mid, last);
        }
    }

    bool Equal(std::size_t i, std::size_t j) const
    {
        return !less_(a_[i], a_[j]) && !less_(a_[j], a_[i]);
    }

    // A partition under way: the range, the pivot's equals gathered in [pFirst, pLast), and how far
    // the scans above and below them have gone.
    struct Cursor
    {
        std::size_t first;
        std::size_t last;
        std::size_t pFirst;
        std::size_t pLast;
        std::size_t gFirst;
        std::size_t gLast;
    };

    // Above the pivots: past what is greater, the pivot's equals swapped in beside it, to the first
    // that is less.
    void ScanAbove(Cursor &c)
    {
        for (; c.gFirst < c.last; ++c.gFirst)
        {
            if (less_(a_[c.pFirst], a_[c.gFirst]))
            {
                continue;
            }
            if (less_(a_[c.gFirst], a_[c.pFirst]))
            {
                break;
            }
            Swap(c.pLast++, c.gFirst);
        }
    }

    // Below them, the same the other way.
    void ScanBelow(Cursor &c)
    {
        for (; c.first < c.gLast; --c.gLast)
        {
            if (less_(a_[c.gLast - 1], a_[c.pFirst]))
            {
                continue;
            }
            if (less_(a_[c.pFirst], a_[c.gLast - 1]))
            {
                break;
            }
            Swap(--c.pFirst, c.gLast - 1);
        }
    }

    // One exchange after the scans: the two out of place swapped, or with no room on one side the
    // pivots moved over by one. True when the scans have met both ends.
    bool Exchange(Cursor &c)
    {
        if (c.gLast == c.first && c.gFirst == c.last)
        {
            return true;
        }
        if (c.gLast == c.first)
        {
            if (c.pLast != c.gFirst)
            {
                Swap(c.pFirst, c.pLast);
            }
            ++c.pLast;
            Swap(c.pFirst++, c.gFirst++);
        }
        else if (c.gFirst == c.last)
        {
            if (--c.gLast != --c.pFirst)
            {
                Swap(c.gLast, c.pFirst);
            }
            Swap(c.pFirst, --c.pLast);
        }
        else
        {
            Swap(c.gFirst++, --c.gLast);
        }
        return false;
    }

    // The range split around a pivot: [first, pivots) before it, [pivots, after) its equals.
    std::pair<std::size_t, std::size_t> Partition(std::size_t first, std::size_t last)
    {
        const std::size_t mid = first + ((last - first) / 2);
        Median(first, mid, last - 1);
        Cursor c{first, last, mid, mid + 1, 0, 0};
        while (first < c.pFirst && Equal(c.pFirst - 1, c.pFirst))
        {
            --c.pFirst;
        }
        while (c.pLast < last && Equal(c.pLast, c.pFirst))
        {
            ++c.pLast;
        }
        c.gFirst = c.pLast;
        c.gLast = c.pFirst;
        do
        {
            ScanAbove(c);
            ScanBelow(c);
        } while (!Exchange(c));
        return {c.pFirst, c.pLast};
    }

    void Insertion(std::size_t first, std::size_t last)
    {
        for (std::size_t next = first + 1; next < last; ++next)
        {
            T value = std::move(a_[next]);
            std::size_t at = next;
            if (less_(value, a_[first]))
            {
                for (; at > first; --at)
                {
                    a_[at] = std::move(a_[at - 1]);
                }
            }
            else
            {
                for (; less_(value, a_[at - 1]); --at)
                {
                    a_[at] = std::move(a_[at - 1]);
                }
            }
            a_[at] = std::move(value);
        }
    }

    void Range(std::size_t first, std::size_t last, std::size_t ideal)
    {
        while (kInsertionMost < last - first && 0 < ideal)
        {
            const auto [pivots, after] = Partition(first, last);
            ideal /= 2;
            ideal += ideal / 2;
            if (pivots - first < last - after)
            {
                Range(first, pivots, ideal);
                first = after;
            }
            else
            {
                Range(after, last, ideal);
                last = pivots;
            }
        }
        if (1 < last - first)
        {
            Insertion(first, last);
        }
    }

    std::vector<T> &a_;
    Less less_;
};

} // namespace vc8

// Returns the places of the items the game takes, in the order it takes them: the first gets the
// highest priority number of its kind (9000 for an add-in in Documents), and so loses to the rest.
// @param items AddIns.xml's or offers.xml's items, in the file's order
// @param enabledOnly true for add-ins, of which the game takes the enabled; false for offers
inline std::vector<std::size_t> AddInOrder(std::span<const AddIn> items, bool enabledOnly)
{
    std::vector<std::size_t> order;
    for (std::size_t i = items.size(); i-- > 0;)
    {
        if (!enabledOnly || items[i].enabled.value_or(false))
        {
            order.push_back(i);
        }
    }
    const auto higher = [&items](std::size_t a, std::size_t b) {
        return items[a].priority.value_or(0) > items[b].priority.value_or(0);
    };
    vc8::Sorter<std::size_t, decltype(higher)>(order, higher).Sort();
    return order;
}

} // namespace dao::core
