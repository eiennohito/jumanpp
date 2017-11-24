//
// Created by Arseny Tolmachev on 2017/02/19.
//

#ifndef JUMANPP_FLATSET_H
#define JUMANPP_FLATSET_H

#include <functional>
#include "flatrep.h"
#include "types.hpp"

namespace jumanpp {
namespace util {

// FlatSet<K,...> provides a set of K.
//
// The map is implemented using an open-addressed hash table.  A
// single array holds entire map contents and collisions are resolved
// by probing at a sequence of locations in the array.
// Imported from TensorFlow.
template <typename Key, class Hash = std::hash<Key>,
          class Eq = std::equal_to<Key>>
class FlatSet {
 private:
  // Forward declare some internal types needed in public section.
  struct Bucket;

 public:
  typedef Key key_type;
  typedef Key value_type;
  typedef Hash hasher;
  typedef Eq key_equal;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef value_type& reference;
  typedef const value_type& const_reference;

  FlatSet() : FlatSet(1) {}

  explicit FlatSet(size_t N, const Hash& hf = Hash(), const Eq& eq = Eq())
      : rep_(N, hf, eq) {}

  FlatSet(const FlatSet& src) : rep_(src.rep_) {}

  template <typename InputIter>
  FlatSet(InputIter first, InputIter last, size_t N = 1,
          const Hash& hf = Hash(), const Eq& eq = Eq())
      : FlatSet(N, hf, eq) {
    insert(first, last);
  }

  FlatSet(std::initializer_list<value_type> init, size_t N = 1,
          const Hash& hf = Hash(), const Eq& eq = Eq())
      : FlatSet(init.begin(), init.end(), N, hf, eq) {}

  FlatSet& operator=(const FlatSet& src) {
    rep_.CopyFrom(src.rep_);
    return *this;
  }

  ~FlatSet() {}

  void swap(FlatSet& x) { rep_.swap(x.rep_); }
  void clear_no_resize() { rep_.clear_no_resize(); }
  void clear() { rep_.clear(); }
  void reserve(size_t N) { rep_.Resize(std::max(N, size())); }
  void rehash(size_t N) { rep_.Resize(std::max(N, size())); }
  void resize(size_t N) { rep_.Resize(std::max(N, size())); }
  size_t size() const { return rep_.size(); }
  bool empty() const { return size() == 0; }
  size_t bucket_count() const { return rep_.bucket_count(); }
  hasher hash_function() const { return rep_.hash_function(); }
  key_equal key_eq() const { return rep_.key_eq(); }

  class const_iterator {
   public:
    typedef typename FlatSet::difference_type difference_type;
    typedef typename FlatSet::value_type value_type;
    typedef typename FlatSet::const_pointer pointer;
    typedef typename FlatSet::const_reference reference;
    typedef ::std::forward_iterator_tag iterator_category;

    const_iterator() : b_(nullptr), end_(nullptr), i_(0) {}

    // Make iterator pointing at first element at or after b.
    const_iterator(Bucket* b, Bucket* end) : b_(b), end_(end), i_(0) {
      SkipUnused();
    }

    // Make iterator pointing exactly at ith element in b, which must exist.
    const_iterator(Bucket* b, Bucket* end, uint32 i)
        : b_(b), end_(end), i_(i) {}

    reference operator*() { return key(); }
    pointer operator->() { return &key(); }
    bool operator==(const const_iterator& x) const {
      return b_ == x.b_ && i_ == x.i_;
    }
    bool operator!=(const const_iterator& x) const { return !(*this == x); }
    const_iterator& operator++() {
      JPP_DCHECK(b_ != end_);
      i_++;
      SkipUnused();
      return *this;
    }
    const_iterator operator++(int /*indicates postfix*/) {
      const_iterator tmp(*this);
      ++*this;
      return tmp;
    }

   private:
    friend class FlatSet;
    Bucket* b_;
    Bucket* end_;
    uint32 i_;

    reference key() const { return b_->key(i_); }
    void SkipUnused() {
      while (b_ < end_) {
        if (i_ >= Rep::kWidth) {
          i_ = 0;
          b_++;
        } else if (b_->marker[i_] < 2) {
          i_++;
        } else {
          break;
        }
      }
    }
  };

  typedef const_iterator iterator;

  iterator begin() { return iterator(rep_.start(), rep_.limit()); }
  iterator end() { return iterator(rep_.limit(), rep_.limit()); }
  const_iterator begin() const {
    return const_iterator(rep_.start(), rep_.limit());
  }
  const_iterator end() const {
    return const_iterator(rep_.limit(), rep_.limit());
  }

  size_t count(const Key& k) const { return rep_.Find(k).found ? 1 : 0; }
  iterator find(const Key& k) {
    auto r = rep_.Find(k);
    return r.found ? iterator(r.b, rep_.limit(), r.index) : end();
  }
  const_iterator find(const Key& k) const {
    auto r = rep_.Find(k);
    return r.found ? const_iterator(r.b, rep_.limit(), r.index) : end();
  }

  bool contains(const Key& k) const {
    auto r = rep_.Find(k);
    return r.found;
  }

  std::pair<iterator, bool> insert(const Key& k) { return Insert(k); }
  template <typename InputIter>
  void insert(InputIter first, InputIter last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    rep_.MaybeResize();
    auto r = rep_.FindOrInsert(std::forward<Args>(args)...);
    const bool inserted = !r.found;
    return {iterator(r.b, rep_.limit(), r.index), inserted};
  }

  size_t erase(const Key& k) {
    auto r = rep_.Find(k);
    if (!r.found) return 0;
    rep_.Erase(r.b, r.index);
    return 1;
  }
  iterator erase(iterator pos) {
    rep_.Erase(pos.b_, pos.i_);
    ++pos;
    return pos;
  }
  iterator erase(iterator pos, iterator last) {
    for (; pos != last; ++pos) {
      rep_.Erase(pos.b_, pos.i_);
    }
    return pos;
  }

  std::pair<iterator, iterator> equal_range(const Key& k) {
    auto pos = find(k);
    if (pos == end()) {
      return std::make_pair(pos, pos);
    } else {
      auto next = pos;
      ++next;
      return std::make_pair(pos, next);
    }
  }
  std::pair<const_iterator, const_iterator> equal_range(const Key& k) const {
    auto pos = find(k);
    if (pos == end()) {
      return std::make_pair(pos, pos);
    } else {
      auto next = pos;
      ++next;
      return std::make_pair(pos, next);
    }
  }

  bool operator==(const FlatSet& x) const {
    if (size() != x.size()) return false;
    for (const auto& elem : x) {
      auto i = find(elem);
      if (i == end()) return false;
    }
    return true;
  }
  bool operator!=(const FlatSet& x) const { return !(*this == x); }

  // If key exists in the table, prefetch it.  This is a hint, and may
  // have no effect.
  void prefetch_value(const Key& key) const { rep_.Prefetch(key); }

 private:
  using Rep = internal::FlatRep<Key, Bucket, Hash, Eq>;

  // Bucket stores kWidth <marker, key, value> triples.
  // The data is organized as three parallel arrays to reduce padding.
  struct Bucket {
    uint8 marker[Rep::kWidth];

    // Wrap keys in union to control construction and destruction.
    union Storage {
      Key key[Rep::kWidth];
      Storage() {}
      ~Storage() {}
    } storage;

    Key& key(uint32 i) {
      JPP_DCHECK_GE(marker[i], 2);
      return storage.key[i];
    }
    void Destroy(uint32 i) { storage.key[i].Key::~Key(); }
    void MoveFrom(uint32 i, Bucket* src, uint32 src_index) {
      new (&storage.key[i]) Key(std::move(src->storage.key[src_index]));
    }
    void CopyFrom(uint32 i, Bucket* src, uint32 src_index) {
      new (&storage.key[i]) Key(src->storage.key[src_index]);
    }
  };

  std::pair<iterator, bool> Insert(const Key& k) {
    rep_.MaybeResize();
    auto r = rep_.FindOrInsert(k);
    const bool inserted = !r.found;
    return {iterator(r.b, rep_.limit(), r.index), inserted};
  }

  Rep rep_;
};

}  // namespace util
}  // namespace jumanpp

#endif  // JUMANPP_FLATSET_H