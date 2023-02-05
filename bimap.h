#pragma once

#include "intrusive_set.h"
#include <cstddef>
#include <type_traits>

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap {
private:
  using left_t = Left;
  using right_t = Right;
  struct tag_for_left;
  struct tag_for_right;

  template <typename T>
  struct template_iterator;

  struct storage_node;

  struct right_struct;

  struct left_struct {
  public:
    struct getter {
      static const left_t& get(const storage_node& storage_node) noexcept {
        return storage_node.left_key;
      }
    };
    using key = left_t;
    using base_node = intrusive::node<tag_for_left>;
    using set = intrusive::intrusive_set<storage_node, key, tag_for_left,
                                         CompareLeft, getter>;
    using iterator = template_iterator<left_struct>;
    using flip_struct = right_struct;
  };

  struct right_struct {
  public:
    struct getter {
      static const right_t& get(const storage_node& storage_node) noexcept {
        return storage_node.right_key;
      }
    };
    using key = right_t;
    using base_node = intrusive::node<tag_for_right>;
    using set = intrusive::intrusive_set<storage_node, key, tag_for_right,
                                         CompareRight, getter>;
    using iterator = template_iterator<right_struct>;
    using flip_struct = left_struct;
  };

  struct bimap_based_node : left_struct::base_node, right_struct::base_node {};

  struct storage_node : bimap_based_node {
  public:
    typename left_struct::key left_key;
    typename right_struct::key right_key;

    template <typename L, typename R>
    storage_node(L&& l, R&& r)
        : left_key(std::forward<L>(l)), right_key(std::forward<R>(r)) {}
  };

  template <class Traits>
  struct template_iterator {
  private:
    friend struct bimap;
    using intr_set_it = typename Traits::set::iterator;
    intr_set_it it;

    template <typename U>
    friend struct template_iterator;

    explicit template_iterator(intr_set_it it_) noexcept : it(it_) {}

  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using getter = typename Traits::getter;
    using value_type = typename Traits::key;
    using pointer = const value_type*;
    using reference = const value_type&;

    template_iterator() = default;

    reference operator*() const noexcept {
      return getter::get(static_cast<storage_node&>(*it));
    };

    pointer operator->() const noexcept {
      return &operator*();
    };

    template_iterator& operator++() noexcept {
      ++it;
      return *this;
    };

    template_iterator operator++(int) noexcept {
      auto tmp = *this;
      operator++();
      return tmp;
    };

    template_iterator& operator--() noexcept {
      --it;
      return *this;
    };

    template_iterator operator--(int) noexcept {
      auto tmp = *this;
      operator--();
      return tmp;
    };

    typename Traits::flip_struct::iterator flip() const noexcept {
      auto* tmp_bimap_based_node = static_cast<bimap_based_node*>(&*it);
      auto* flipped_base_node =
          static_cast<typename Traits::flip_struct::base_node*>(
              tmp_bimap_based_node);
      typename Traits::flip_struct::set::iterator tmp_it(flipped_base_node);
      return template_iterator<typename Traits::flip_struct>(tmp_it);
    };

    friend bool operator==(const template_iterator& left,
                           const template_iterator& right) noexcept {
      return left.it == right.it;
    }

    friend bool operator!=(const template_iterator& left,
                           const template_iterator& right) noexcept {
      return left.it != right.it;
    }
  };

public:
  using right_iterator = typename right_struct::iterator;

  using left_iterator = typename left_struct::iterator;

  bimap(CompareLeft compare_left = CompareLeft(),
        CompareRight compare_right = CompareRight())
      : left_set(sentinel, std::move(compare_left)),
        right_set(sentinel, std::move(compare_right)) {}

  void swap(bimap& other) noexcept {
    left_set.swap(other.left_set);
    right_set.swap(other.right_set);
    std::swap(m_size, other.m_size);
  }

  bimap(const bimap& other)
      : left_set(sentinel, static_cast<CompareLeft>(other.left_set)),
        right_set(sentinel, static_cast<CompareRight>(other.right_set)) {
    for (auto it = other.begin_left(); it != other.end_left(); it++) {
      insert(*it, *it.flip());
    }
  };

  bimap& operator=(const bimap& other) {
    if (&other != this) {
      bimap(other).swap(*this);
    }
    return *this;
  };

  ~bimap() noexcept {
    erase_left(begin_left(), end_left());
  };

private:
  template <typename L, typename R>
  left_iterator perfect_forwarding_insert(L&& left, R&& right) {
    if (!(find_left(left) == end_left() && find_right(right) == end_right())) {
      return end_left();
    }
    auto* storage =
        new storage_node(std::forward<L>(left), std::forward<R>(right));
    right_set.insert(*storage, true);
    m_size++;
    return left_iterator(left_set.insert(*storage, true));
  }

public:
  left_iterator insert(const left_t& left, const right_t& right) {
    return perfect_forwarding_insert(left, right);
  }
  left_iterator insert(const left_t& left, right_t&& right) {
    return perfect_forwarding_insert(left, std::move(right));
  };
  left_iterator insert(left_t&& left, const right_t& right) {
    return perfect_forwarding_insert(std::move(left), right);
  };
  left_iterator insert(left_t&& left, right_t&& right) {
    return perfect_forwarding_insert(std::move(left), std::move(right));
  };

  left_iterator erase_left(left_iterator it) noexcept {
    right_set.erase(it.flip().it);
    auto copy = it++;
    auto next = left_iterator(it.it);
    delete static_cast<storage_node*>(left_set.erase(copy.it));
    m_size--;
    return next;
  };

  bool erase_left(const left_t& left) noexcept {
    auto it = find_left(left);
    if (it != end_left()) {
      erase_left(it);
      return true;
    }
    return false;
  };

  right_iterator erase_right(right_iterator it) noexcept {
    left_set.erase(it.flip().it);
    auto copy = it++;
    auto next = right_iterator(it.it);
    delete static_cast<storage_node*>(right_set.erase(copy.it));
    m_size--;
    return next;
  };

  bool erase_right(const right_t& right) noexcept {
    auto it = find_right(right);
    if (it != end_right()) {
      erase_right(it);
      return true;
    }
    return false;
  };

  left_iterator erase_left(left_iterator first, left_iterator last) noexcept {
    for (auto it = first; it != last; it = erase_left(it)) {}
    return last;
  };

  right_iterator erase_right(right_iterator first,
                             right_iterator last) noexcept {
    for (auto it = first; it != last; it = erase_right(it)) {}
    return last;
  };

  left_iterator find_left(const left_t& left) const noexcept {
    return left_iterator(left_set.find(left));
  };
  right_iterator find_right(const right_t& right) const noexcept {
    return right_iterator(right_set.find(right));
  };

  right_t const& at_left(const left_t& key) const {
    auto left_it = find_left(key);
    if (left_it == end_left()) {
      throw std::out_of_range("element doesn't exist");
    }
    return *left_it.flip();
  };

  left_t const& at_right(const right_t& key) const {
    auto right_it = find_right(key);
    if (right_it == end_right()) {
      throw std::out_of_range("element doesn't exist");
    }
    return *right_it.flip();
  };

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует, добавляет его в bimap и на противоположную
  // сторону кладет дефолтный элемент, ссылку на который и возвращает
  // Если дефолтный элемент уже лежит в противоположной паре - должен поменять
  // соответствующий ему элемент на запрашиваемый (смотри тесты)

  template <typename Q = right_t,
            typename = std::enable_if_t<std::is_default_constructible_v<Q>>>
  right_t const& at_left_or_default(const left_t& key) {
    if (auto it = find_left(key); it != end_left()) {
      return *it.flip();
    }
    right_t tmp{};
    if (auto it = find_right(tmp); it != end_right()) {
      erase_right(it);
    }
    return *insert(key, std::move(tmp)).flip();
  }

  template <typename Q = left_t,
            typename = std::enable_if_t<std::is_default_constructible_v<Q>>>
  left_t const& at_right_or_default(const right_t& key) {
    if (auto it = find_right(key); it != end_right()) {
      return *it.flip();
    }
    left_t tmp{};
    if (auto it = find_left(tmp); it != end_left()) {
      erase_left(it);
    }
    return *insert(std::move(tmp), key);
  }

  left_iterator lower_bound_left(const left_t& left) const noexcept {
    return left_iterator(left_set.lower_bound(left));
  };

  left_iterator upper_bound_left(const left_t& left) const noexcept {
    return left_iterator(left_set.upper_bound(left));
  };

  right_iterator lower_bound_right(const right_t& right) const noexcept {
    return right_iterator(right_set.lower_bound(right));
  };

  right_iterator upper_bound_right(const right_t& right) const noexcept {
    return right_iterator(right_set.upper_bound(right));
  };

  left_iterator begin_left() const noexcept {
    return left_iterator(left_set.begin());
  };

  left_iterator end_left() const noexcept {
    return left_iterator(left_set.end());
  };

  right_iterator begin_right() const noexcept {
    return right_iterator(right_set.begin());
  };

  right_iterator end_right() const noexcept {
    return right_iterator(right_set.end());
  };

  bool empty() const noexcept {
    return m_size == 0;
  };

  std::size_t size() const noexcept {
    return m_size;
  };

  friend bool operator==(bimap const& a, bimap const& b) noexcept {
    if (a.size() != b.size()) {
      return false;
    }

    const CompareLeft& comp_left = a.left_set;
    const CompareRight& comp_right = a.right_set;

    for (auto it1 = a.begin_left(), it2 = b.begin_left(); it2 != b.end_left();
         it1++, it2++) {
      if (comp_left(*it1, *it2) || comp_left(*it2, *it1) ||
          comp_right(*it1.flip(), *it2.flip()) ||
          comp_right(*it2.flip(), *it1.flip())) {
        return false;
      }
    }
    return true;
  };

  friend bool operator!=(bimap const& a, bimap const& b) noexcept {
    return !(a == b);
  };

private:
  bimap_based_node sentinel;
  std::size_t m_size{};
  typename left_struct::set left_set;
  typename right_struct::set right_set;
};
