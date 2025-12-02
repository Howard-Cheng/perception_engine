#include "pe_base/transport/data_buffer.h"

#include <cstring>

#include "pe_base/logger.h"

namespace pe_base {
DataBuffer::DataBuffer() : offset_(0), size_(0), capacity_(0), data_(nullptr) {
  PE_CHECK(!IsConsistent(), "")
}

DataBuffer::DataBuffer(DataBuffer&& buf) noexcept
    : offset_(0),
      size_(buf.size_),
      capacity_(buf.capacity_),
      data_(std::move(buf.data_)) {
  PE_CHECK(!IsConsistent(), "")
  buf.OnMovedFrom();
}

DataBuffer::DataBuffer(const uint8_t* data, size_t size, size_t capacity)
    : DataBuffer(size, capacity) {
  PE_CHECK_RET(!IsConsistent(), "", )
  std::memcpy(data_.get(), data, size);
}

DataBuffer::DataBuffer(size_t size, size_t capacity)
    : offset_(0),
      size_(size),
      capacity_((std::max)(size, capacity)),
      data_(capacity_ > 0 ? new uint8_t[capacity_]
                          : nullptr){PE_CHECK(!IsConsistent(), "")}

      DataBuffer::DataBuffer(size_t size)
    : DataBuffer(size, size) {}

DataBuffer::DataBuffer(const uint8_t* data, size_t size)
    : DataBuffer(data, size, size) {}

DataBuffer::DataBuffer(const DataBuffer& buf)
    : DataBuffer(buf.Data_(), buf.size_) {}

uint8_t* DataBuffer::Data_() {
  PE_CHECK_RET(!IsConsistent(), "", nullptr)
  return data_.get() + offset_;
}

const uint8_t* DataBuffer::Data_() const {
  PE_CHECK_RET(!IsConsistent(), "", nullptr)
  return data_.get() + offset_;
}

bool DataBuffer::Empty() const {
  PE_CHECK_RET(!IsConsistent(), "", true)
  return size_ == 0;
}

size_t DataBuffer::Offset_() const { return offset_; }

size_t DataBuffer::Size_() const {
  PE_CHECK_RET(!IsConsistent(), "", 0)
  return size_;
}

size_t DataBuffer::Capacity_() const {
  PE_CHECK_RET(!IsConsistent(), "", 0)
  return capacity_;
}

DataBuffer& DataBuffer::operator=(const DataBuffer& other) {
  SetData(other.Data_(), other.Size_());
  PE_CHECK(!IsConsistent(), "")
  return *this;
}

uint8_t& DataBuffer::operator[](size_t index) {
  PE_CHECK(index >= capacity_, "")
  return Data_()[index];
}

uint8_t DataBuffer::operator[](size_t index) const {
  PE_CHECK(index >= capacity_, "")
  return Data_()[index];
}

uint8_t* DataBuffer::begin() { return Data_(); }

const uint8_t* DataBuffer::begin() const { return Data_(); }

uint8_t* DataBuffer::end() { return Data_() + Size_(); }

const uint8_t* DataBuffer::end() const { return Data_() + Size_(); }

const uint8_t* DataBuffer::cbegin() const { return Data_(); }

const uint8_t* DataBuffer::cend() const { return Data_() + Size_(); }

void DataBuffer::SetData(const uint8_t* data, size_t size) {
  PE_CHECK_RET(!IsConsistent(), "", )
  offset_ = 0;
  size_ = 0;
  AppendData(data, size);
}

void DataBuffer::AppendData(const uint8_t* data, size_t size) {
  PE_CHECK_RET(!IsConsistent(), "", )
  const size_t new_size = size_ + size;
  EnsureCapacityWithHeadroom(offset_ + new_size, true);
  std::memcpy(data_.get() + offset_ + size_, data, size);
  size_ = new_size;
  PE_CHECK_RET(!IsConsistent(), "", )
}

void DataBuffer::AppendData(const uint8_t& data) { AppendData(&data, 1); }

void DataBuffer::SetOffset_(size_t offset, size_t size) {
  EnsureCapacityWithHeadroom(offset + size, true);
  offset_ = offset;
  size_ = size;
}

void DataBuffer::SetSize_(size_t size) {
  EnsureCapacityWithHeadroom(offset_ + size, true);
  size_ = size;
}

void DataBuffer::EnsureCapacity(size_t capacity) {
  EnsureCapacityWithHeadroom(capacity, false);
}

void DataBuffer::Clear() {
  offset_ = 0;
  size_ = 0;
  PE_CHECK_RET(!IsConsistent(), "", )
}

bool DataBuffer::operator!=(const DataBuffer& buf) const {
  return !(*this == buf);
}

bool DataBuffer::operator==(const DataBuffer& buf) const {
  PE_CHECK_RET(!IsConsistent(), "", false)
  if (size_ != buf.size_) {
    return false;
  }
  return std::memcmp(data_.get() + offset_, buf.data_.get() + buf.offset_,
                     size_) == 0;
}

DataBuffer& DataBuffer::operator=(DataBuffer&& other) noexcept {
  PE_CHECK(!other.IsConsistent(), "")
  offset_ = other.offset_;
  size_ = other.size_;
  capacity_ = other.capacity_;

  std::swap(data_, other.data_);
  other.data_.reset();
  other.OnMovedFrom();
  return *this;
}

bool DataBuffer::IsConsistent() const {
  return (data_ || capacity_ == 0) && capacity_ >= size_ + offset_;
}

void DataBuffer::OnMovedFrom() {
  offset_ = 0;
  size_ = 0;
  capacity_ = 0;
}

void DataBuffer::EnsureCapacityWithHeadroom(size_t capacity,
                                            bool extra_headroom) {
  PE_CHECK_RET(!IsConsistent(), "", )
  if (capacity <= capacity_) return;

  const size_t new_capacity =
      extra_headroom ? (std::max)(capacity, capacity_ + capacity_ / 2)
                     : capacity;

  std::unique_ptr<uint8_t[]> new_data(new uint8_t[new_capacity]);
  if (data_ != nullptr) {
    std::memcpy(new_data.get(), data_.get(), size_);
  }
  data_ = std::move(new_data);
  capacity_ = new_capacity;
  PE_CHECK_RET(!IsConsistent(), "", )
}

void swap(DataBuffer& a, DataBuffer& b) {
  std::swap(a.offset_, b.offset_);
  std::swap(a.size_, b.size_);
  std::swap(a.capacity_, b.capacity_);
  std::swap(a.data_, b.data_);
}
const char* DataBuffer::ToString() const {
  if (size_ == 0 || !data_) return "";
  if (strnlen((char*)data_.get() + offset_, size_) >= size_) return "";
  return (char*)data_.get() + offset_;
}
}  // namespace pe_base
