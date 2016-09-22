#ifndef BROADCAST_HPP
#define BROADCAST_HPP

#include <utility>
#include <tuple>
#include <type_traits>
#include <stdexcept>
#include <iterator>
#include <array>
#include <algorithm>

#include "xindex.hpp"
#include "utils.hpp"

namespace qs
{

    /*************************
     * Broadcast functions
     *************************/

    template <class S, size_t N>
    S broadcast_dim(std::array<S, N>& dim_list);

    template <class S>
    bool broadcast_shape(const array_shape<S>& input, array_shape<S>& output);

    template <class S>
    bool check_trivial_broadcast(const array_strides<S>& strides1,
                                 const array_strides<S>& strides2);


    /***************************
     * broadcasting_iterator
     ***************************/

    // TODO : refactor this
    template <class I, class S>
    class broadcasting_iterator
    {

    public:

        
        using subiterator_type = I;
        using value_type = typename subiterator_type::value_type;
        using reference = typename subiterator_type::reference;
        using pointer = typename subiterator_type::pointer;
        using difference_type = typename subiterator_type::difference_type;
        using iterator_category = std::forward_iterator_tag;

        using size_type = S;
        using shape_type = array_shape<size_type>;
        using strides_type = array_strides<size_type>;

        broadcasting_iterator(subiterator_type it, strides_type&& strides, const shape_type& shape);
        reference operator*() const;

        void increment(size_type i);
        void reset(size_type i);

    private:

        subiterator_type m_iter;
        strides_type m_strides;
        strides_type m_backstrides;
    };


    /********************
     * multi_iterator
     ********************/

    template <class S, class... I>
    class multi_iterator
    {

    public:

        using iterator_container = std::tuple<I...>;
        
        template <size_t N>
        using iterator = typename std::tuple_element<N, iterator_container>::type;
        
        template <size_t N>
        using reference = typename iterator<N>::reference;

        using size_type = S;
        using shape_type = array_shape<S>;
        
        multi_iterator(I&&... iterator, const shape_type& shape);

        multi_iterator& operator++();

        template <size_t N>
        reference<N> data() const;

    private:

        void increment(size_type i);
        void reset(size_type i);

        iterator_container m_iterator;
        shape_type m_shape;
        shape_type m_index;
    };


    /****************************************
     * Broadcast functions implementation
     ****************************************/

    template <class S, size_t N>
    inline S broadcast_dim(const std::array<S, N>& dim_list)
    {
        S ndim = std::accumulate(dim_list.begin(), dim_list.end(), S(0),
                [](S res, const S& dim) { return std::max(dim, res); });
        return ndim;
    }

    template <class S>
    inline bool broadcast_shape(const array_shape<S>& input, array_shape<S>& output)
    {
        size_t size = output.size();
        bool trivial_broadcast = (input.size() == output.size());
        auto output_iter = output.rbegin();
        for(auto input_iter = input.rbegin(); input_iter != input.rend();
            ++input_iter, ++output_iter)
        {
            if(*output_iter == 1)
            {
                *output_iter = *input_iter;
            }
            else if((*input_iter != 1) && (*output_iter != *input_iter))
            {
                throw std::runtime_error("broadcast error : incompatible dimension of inputs");
            }
            trivial_broadcast = trivial_broadcast && (*output_iter == *input_iter);
        }
        return trivial_broadcast;
    }

    template <class S>
    inline bool check_trivial_broadcast(const array_strides<S>& strides1,
                                        const array_strides<S>& strides2)
    {
        return strides1 == strides2;
    }


    /******************************************
     * broadcasting_iterator implementation
     ******************************************/

    template <class I, class S>
    inline broadcasting_iterator<I, S>::broadcasting_iterator(subiterator_type iter,
                                                              strides_type&& strides,
                                                              const shape_type& shape)
        : m_iter(iter), m_strides(std::move(strides)), m_backstrides(shape.size())
    {
        std::transform(strides.begin(), strides.end(), shape.begin(), m_backstrides.begin(),
                [](size_type stride, size_type shape) { return stride != 0 ? stride * shape - 1 : 0;});
    }

    template <class I, class S>
    inline auto broadcasting_iterator<I, S>::operator*() const -> reference
    {
        return *m_iter;
    }

    template <class I, class S>
    inline void broadcasting_iterator<I, S>::increment(size_type dim)
    {
        m_iter += m_strides[dim];
    }

    template <class I, class S>
    inline void broadcasting_iterator<I, S>::reset(size_type dim)
    {
        m_iter -= m_backstrides[dim];
    }


    /***********************************
     * multi_iterator implementation
     ***********************************/

    template <class S, class... I>
    inline multi_iterator<S, I...>::multi_iterator(I&&... iterator, const shape_type& shape)
        : m_iterator(std::make_tuple(iterator...)), m_shape(shape), m_index(shape.size(), S(0))
    {
    }

    template <class S, class... I>
    inline multi_iterator<S, I...>& multi_iterator<S, I...>::operator++()
    {
        for(size_type j = m_index.size(); j != 0; --j)
        {
            size_type i = j-1;
            if(++m_index[i] != m_shape[i])
            {
                increment(i);
                break;
            }
            else
            {
                m_index[i] = 0;
                reset(i);
            }
        }
    }

    template <class S, class... I>
    template <size_t N>
    inline auto multi_iterator<S, I...>::data() const -> reference<N>
    {
        return std::get<N>(m_iterator);
    }

    template <class S, class... I>
    inline void multi_iterator<S, I...>::increment(size_type i)
    {
        auto fn = [i](auto& iter) { iter.increment(i); };
        for_each(fn, m_iterator);
    }

    template <class S, class... I>
    inline void multi_iterator<S, I...>::reset(size_type i)
    {
        auto fn = [i](auto& iter) { iter.reset(i); };
        for_each(fn, m_iterator);
    }
}

#endif
