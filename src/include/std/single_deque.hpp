#pragma once

/// Credit: Qwinci

namespace kstd {
    struct SingleNode {
        void* next;
    };
 
    template<typename T, SingleNode T::* Hook>
    class SingleDeque {
        private:
            T* head {};
            T* tail {};

        public:
            class Iterator {
                private:
                    T* ptr;
                    T* next;
                    friend class SingleDeque;

                    constexpr explicit Iterator(T* start)
                        : ptr(start),
                          next(start ? static_cast<T*>((start->*Hook).next) : nullptr) {}

                public:
                    constexpr bool operator!=(const Iterator& other) const {
                        return ptr != other.ptr;
                    }
                    
                    constexpr Iterator& operator++() {
                        ptr = next;
                        if (ptr) {
                            next = static_cast<T*>((ptr->*Hook).next);
                        }

                        return *this;
                    }

                    constexpr T& operator*() const {
                        return *ptr;
                    }

                    constexpr T* operator->() const {
                        return ptr;
                    }
            };

            constexpr bool empty() {
                return head == nullptr && tail == nullptr;
            }

            constexpr void clear_all() {
                head = nullptr;
                tail = nullptr;
            }

            constexpr Iterator begin() { 
                return Iterator(head); 
            }

            constexpr Iterator end() { 
                return Iterator(nullptr); 
            }

            constexpr void push_head(T* value) {
                if (head == nullptr) {
                    tail = value;
                    head = value;
                    return;
                }

                (value->*Hook).next = head;
                head = value;
            }

            constexpr void push_tail(T* value) {
                if (tail == nullptr) {
                    head = value;
                    tail = value;
                    return;
                }
                

                (tail->*Hook).next = value;
                tail = value;
            }

            constexpr T* pop_head() {
                if (head == nullptr) {
                    return nullptr;
                }

                auto head_val = head;
                head = static_cast<T*>((head->*Hook).next);
                if (head == nullptr) {
                    tail = nullptr;
                }

                return head_val;
            }

            constexpr T* pop_tail() {
                if (!tail) {
                    return nullptr;            
                }

                auto tail_val = tail;
               
                if (head != tail) {
                    auto new_tail = head; 

                    while (static_cast<T*>((new_tail->*Hook).next) != tail) {
                        new_tail = static_cast<T*>((new_tail->*Hook).next);
                    }

                    (new_tail->*Hook).next = nullptr;
                    tail = new_tail;
                }

                return tail_val;
            }
    };
}
