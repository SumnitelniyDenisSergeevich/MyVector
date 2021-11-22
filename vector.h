#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept { 
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept { 
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(begin(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.begin(), other.size_, begin());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0))
    {
    }

    ~Vector() {
        std::destroy_n(begin(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (rhs.size_ < size_) {
                    std::copy_n(rhs.begin(), rhs.size_, begin());
                    std::destroy_n(begin() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy_n(rhs.begin(), size_, begin());
                    std::uninitialized_copy_n(rhs.begin() + size_, rhs.size_ - size_, end());
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_  = std::exchange(rhs.size_, 0);
        }
        return *this;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        InitializeWithCopyMoveUninitializedN(begin(), size_,new_data.GetAddress());
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size){
        if (new_size < size_) {
            std::destroy_n(begin() + new_size, size_ - new_size);
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(end(), new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(value);
            InitializeWithCopyMoveUninitializedN(begin(), size_, new_data.GetAddress());
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(value);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::move(value));
            InitializeWithCopyMoveUninitializedN(begin(), size_, new_data.GetAddress());
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(std::move(value));
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            InitializeWithCopyMoveUninitializedN(begin(), size_, new_data.GetAddress());
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return  *(end()-1);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        auto iter = pos - begin();
        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + iter) T(std::forward<Args>(args)...);
            try {
                InitializeWithCopyMoveUninitializedN(begin(), iter, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_at(new_data.GetAddress() + iter);
                throw;
            }
            try {
                InitializeWithCopyMoveUninitializedN(begin() + iter, size_ - iter, new_data.GetAddress() + iter + 1);
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress(), iter + 1);
                throw;
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            T temp_obj = T(std::forward<Args>(args)...);
            std::uninitialized_move_n(end() - 1, 1, end());
            std::move_backward(begin() + iter, end() - 1, end());
            data_[iter] = std::move(temp_obj);
        }
        ++size_;
        return begin()+iter;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        auto iter = pos - begin();
        std::move(begin() + iter + 1, end(), begin() + iter);
        std::destroy_at(end() - 1);
        --size_;
        return begin() + iter;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    void PopBack() noexcept {
        std::destroy_at(end() - 1);
        --size_;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];   
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    void InitializeWithCopyMoveUninitializedN(iterator from, size_t count, iterator to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, count, to);
        }
        else {
            std::uninitialized_copy_n(from, count, to);
        }
    }


    RawMemory<T> data_;
    size_t size_ = 0;
};