#ifndef MPC_SMALL_VECTOR
#define MPC_SMALL_VECTOR

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace mpc {

    template <typename T, size_t N>
    class small_vector {
    public:
        //Definitions
        typedef T value_type;
        typedef value_type & reference;
        typedef value_type const& const_reference;
        typedef value_type * pointer;
        typedef pointer const const_pointer;
        typedef value_type && forward_reference;
        typedef value_type * iterator;
        typedef value_type const* const_iterator;
        typedef std::aligned_storage_t<sizeof(T), alignof(T)> storage;
        typedef size_t size_type;

        static constexpr bool is_nothrow_movable = std::is_nothrow_move_constructible<value_type>::value && std::is_nothrow_move_assignable<value_type>::value;
        static constexpr bool is_nothrow_swappable = is_nothrow_movable &&
            std::is_nothrow_move_constructible<storage>::value && std::is_nothrow_move_assignable<storage>::value &&
            std::is_nothrow_move_constructible<size_type>::value && std::is_nothrow_move_assignable<size_type>::value;

        //Constructors
        small_vector() noexcept: m_size(0), m_capacity(N), m_data(buffer()) {}

        small_vector(const small_vector & other): small_vector() {
            if (other.size() > m_capacity) {
                reserve(other.size());
            }
            for (size_type i = 0; i < other.size(); ++i) {
                push_back(other[i]);
            }
        }

        small_vector(small_vector && other) noexcept(is_nothrow_movable): m_size(other.size()), m_capacity(other.capacity()), m_data(buffer()) {
            //If other vector uses buffer for storage, swap buffers
            if (other.uses_buffer()) {
                std::swap(m_buffer, other.m_buffer);
            //Dynamic memory (swap pointers)
            } else {
                m_data = other.m_data;
            }
            other.m_data = other.buffer();
            other.m_size = 0;
            other.m_capacity = N;
        }

        small_vector(std::initializer_list<value_type> init):
            m_size(init.size()),
            m_capacity(m_size), 
            m_data(
                m_capacity > N ?
                (pointer)::operator new(m_capacity * sizeof(value_type)) :
                buffer()
            )
        {
            try {
                std::uninitialized_copy(init.begin(), init.end(), m_data);
            } catch (...) {
                if (m_data != buffer()) {
                    ::operator delete(m_data);
                }
                throw;
            }
        }

        //Getters
        reference operator[](size_type pos) {
            return m_data[pos];
        }

        const_reference operator[](size_type pos) const {
            return m_data[pos];
        }

        //Content modification
        void push_back(const_reference item) {
            emplace_back(item);
        }

        void push_back(forward_reference item) {
            emplace_back(std::move(item));
        }

        template<typename... Args>
        reference emplace_back(Args&&... args) {
            if (m_size >= m_capacity) {
                reserve(m_capacity == 0 ? 1 : 2 * m_capacity);
            }
            new (m_data + m_size) value_type(std::forward<Args>(args)...);            
            return m_data[m_size++];
        }

        small_vector& operator=(small_vector other) noexcept(is_nothrow_swappable) {
            //Unifying operator
            swap(other);
            return *this;
        }

        void clear() noexcept {
            for (size_type i = 0; i < m_size; ++i) {
                std::launder(reinterpret_cast<pointer>(&m_data[i]))->~value_type();
            }
            m_size = 0;
        }

        void swap(small_vector & other) noexcept(is_nothrow_swappable) {
            //Both use buffer, swap buffers
            if (uses_buffer() && other.uses_buffer()) {
                std::swap(m_buffer, other.m_buffer);
            //First uses buffer, other doesn't -> other uses buffer, first doesn't
            } else if (uses_buffer()) {
                std::swap(m_buffer, other.m_buffer);
                m_data = other.m_data;
                other.m_data = other.buffer();
            //Other uses buffer, first doesn't -> first uses buffer, other doesn't
            } else if (other.uses_buffer()) {
                std::swap(m_buffer, other.m_buffer);
                other.m_data = m_data;
                m_data = buffer();
            //Only swap pointers (dynamic only)
            } else {
                std::swap(m_data, other.m_data);
            }
            //Always swap capacity and size
            std::swap(m_capacity, other.m_capacity);
            std::swap(m_size, other.m_size);
        }

        void resize(size_type size, const_reference value = value_type()) {
            if (size == m_size) {
                return;
            }
            //Alloc new data and copy value_type into it
            if (size > m_size) {
                reserve(size);
                for (size_type i = m_size; i < size; ++i) {
                    push_back(value);
                }
            //Destroy padded data - size < m_size
            } else {
                while (m_size > size) {
                    m_data[--m_size].~value_type();
                }
            }
        }

        //Other setters
        void reserve(size_type capacity) {
            if (capacity <= m_capacity) {
                return;
            }
            pointer tmp = (pointer)::operator new(capacity * sizeof(value_type));
            size_type i = 0;
            try {
                for (; i < m_size; ++i) {
                    new (tmp + i) value_type(std::move_if_noexcept(m_data[i]));
                }
            } catch (...) {
                for (; i > 0; --i) {
                    (tmp + i - 1)->~value_type();
                    ::operator delete(tmp);
                    throw;
                }
            }
            clear();
            if (!uses_buffer()) {
                ::operator delete(m_data);
            }
            m_data = tmp;
            m_capacity = capacity;
            m_size = i;
        }

        //Other getters
        size_type size() const noexcept {
            return m_size;
        }

        size_type capacity() const noexcept {
            return m_capacity;
        }

        pointer data() const noexcept {
            return m_data;
        }

        iterator begin() noexcept {
            return m_data;
        }

        const_iterator begin() const noexcept {
            return m_data;
        }

        iterator end() noexcept {
            return m_data + m_size;
        }

        const_iterator end() const noexcept {
            return m_data + m_size;
        }

        //Destructors
        ~small_vector() noexcept {
            clear();
            if (!uses_buffer()) {
                ::operator delete(m_data);
            }
        }
    private:
        //Helpers
        inline bool uses_buffer() noexcept {
            return m_size <= N;
        }

        inline pointer buffer() noexcept {
            return reinterpret_cast<pointer>(m_buffer);
        }

        //Data
        size_type m_size;
        size_type m_capacity;

        pointer m_data;
        std::aligned_storage_t<sizeof(T), alignof(T)> m_buffer[N];
    };

    template <typename T, size_t N>
    void swap(small_vector<T,N> & a, small_vector<T,N> & b) noexcept(mpc::small_vector<T,N>::is_nothrow_swappable) {
        a.swap(b);
    }
}

#endif // MPC_SMALL_VECTOR
