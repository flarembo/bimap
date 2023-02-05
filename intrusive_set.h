#pragma once

#include <iterator>

namespace intrusive {

namespace details {
template <typename T, typename Key>
struct default_getter {
  static const Key& get(const T& t) noexcept {
    return t.key;
  }
};
} // namespace details

template <typename T, typename Key, typename Tag, typename Compare,
          typename Getter>
struct intrusive_set;

struct default_tag;

template <typename Tag = default_tag>
struct node {
public:
  node() noexcept = default;

  ~node() noexcept = default;

  node(const node&) = delete;

  node(node&&) noexcept = default;

  node& operator=(const node&) = delete;

  node& operator=(node&&) noexcept = default;

  static void set_child(node* old_node, node* new_node) noexcept {
    auto parent = old_node->parent;
    if (parent->left == old_node) {
      parent->left = new_node;
    } else {
      parent->right = new_node;
    }
    if (new_node) {
      new_node->parent = parent;
    }
  }

private:
  node* parent{nullptr};
  node* left{nullptr};
  node* right{nullptr};

  template <typename T, typename Key, typename STag, typename Compare,
            typename Getter>
  friend struct intrusive_set;
};

template <typename T, typename Key, typename Tag = default_tag,
          typename Compare = std::less<Key>,
          typename Getter = details::default_getter<T, Key>>
struct intrusive_set : Compare {
private:
  using node_t = node<Tag>;

  node_t* sentinel = nullptr;

  const Key& get_key(const node_t* node) const noexcept {
    return Getter::get(*static_cast<const T*>(node));
  }

  bool less(const Key& left, const Key& right) const noexcept {
    return Compare::operator()(left, right);
  }

  bool greater(const Key& left, const Key& right) const noexcept {
    return Compare::operator()(right, left);
  }

  bool equals(const Key& left, const Key& right) const noexcept {
    return !(greater(left, right) || less(left, right));
  }

public:
  explicit intrusive_set(node_t& sentinel_,
                         Compare&& compare = Compare()) noexcept
      : Compare(std::move(compare)), sentinel(&sentinel_){};

  ~intrusive_set() noexcept = default;

  intrusive_set(const intrusive_set&) = delete;

  intrusive_set& operator=(const intrusive_set&) = delete;

  void swap(intrusive_set& other) noexcept {
    std::swap(static_cast<Compare&>(*this), static_cast<Compare&>(other));
    std::swap(sentinel->left, other.sentinel->left); // need this for bimap
    if (sentinel->left) {
      sentinel->left->parent = sentinel;
    }
    if (other.sentinel->left) {
      other.sentinel->left->parent = other.sentinel;
    }
  }

  intrusive_set(intrusive_set&& other) noexcept
      : Compare(std::move(other)), sentinel() {
    sentinel->left = other.sentinel->left;
    if (sentinel->left) {
      sentinel->left->parent = sentinel;
    }
    other.sentinel->left = nullptr;
  }

  intrusive_set& operator=(intrusive_set&& other) noexcept {
    if (this != &other) {
      intrusive_set(std::move(other)).swap(*this);
    }
    return *this;
  }

  struct iterator {
  private:
    friend intrusive_set;
    node_t* node = nullptr;

  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = node_t;
    using pointer = node_t*;
    using reference = node_t&;

    explicit iterator(node_t* node_) noexcept : node(node_) {}

    iterator() noexcept = default;

    reference operator*() const noexcept {
      return *node;
    }

    pointer operator->() const noexcept {
      return node;
    }

    iterator& operator++() noexcept {
      if (node->right) {
        node = node->right;
        while (node->left) {
          node = node->left;
        }
        return *this;
      }
      while (node->parent && node->parent->right == node) {
        node = node->parent;
      }
      node = node->parent;
      return *this;
    }

    iterator operator++(int) noexcept {
      iterator tmp = *this;
      operator++();
      return tmp;
    }

    iterator& operator--() noexcept {
      if (node->left) {
        node = node->left;
        while (node->right) {
          node = node->right;
        }
        return *this;
      }
      while (node->parent && node->parent->left == node) {
        node = node->parent;
      }
      node = node->parent;
      return *this;
    }

    iterator operator--(int) noexcept {
      auto tmp = *this;
      operator--();
      return tmp;
    }

    bool operator==(iterator other) const noexcept {
      return other.node == node;
    }

    bool operator!=(iterator other) const noexcept {
      return other.node != node;
    }
  };

  iterator insert(T& obj, bool hint = false) noexcept {
    if (!hint) {
      auto it = find(Getter::get(obj));
      if (it != end()) {
        return end();
      }
    }
    sentinel->left = add_to_tree(obj, sentinel->left);
    sentinel->left->parent = sentinel;
    return iterator(&obj);
  }

  T* erase(iterator it) noexcept {
    auto node = it.node;
    if (node->left && node->right) {
      auto new_node = node->left;
      while (new_node->right) {
        new_node = new_node->right;
      }
      if (new_node->parent != node) {
        node_t::set_child(new_node, new_node->left);
        new_node->left = node->left;
        new_node->left->parent = new_node;
      }
      new_node->right = node->right;
      node->right->parent = new_node;
      node_t::set_child(node, new_node);
    } else {
      node_t::set_child(node, node->left ? node->left : node->right);
    }
    node->left = node->right = node->parent = nullptr;
    return static_cast<T*>(node);
  }

  iterator lower_bound(const Key& key) const noexcept {
    return iterator(bound_impl(key, sentinel->left));
  }

  iterator upper_bound(const Key& key) const noexcept {
    auto it = lower_bound(key);
    if (it != end() && equals(key, get_key(it.node))) {
      ++it;
    }
    return it;
  }

  iterator find(const Key& key) const noexcept {
    auto it = lower_bound(key);
    // need compare cause cast (get_key) sentinel is UB. In bimap sentinel
    // doesn't have key field
    return it != end() && equals(key, get_key(it.node)) ? it : end();
  }

  iterator begin() const noexcept {
    auto node = sentinel;
    while (node->left) {
      node = node->left;
    }
    return iterator(node);
  }

  iterator end() const noexcept {
    return iterator(sentinel);
  }

private:
  node_t* bound_impl(const Key& key, node_t* node) const noexcept {
    if (!node) {
      return sentinel;
    }

    if (less(get_key(node), key)) {
      return bound_impl(key, node->right);
    }

    if (greater(get_key(node), key)) {
      auto tmp = bound_impl(key, node->left);
      if (tmp != sentinel) {
        return tmp;
      }
    }
    return node;
  }

  node_t* add_to_tree(T& obj, node_t* node) noexcept {
    if (!node) {
      return static_cast<node_t*>(&obj);
    }
    if (less(Getter::get(obj), get_key(node))) {
      node->left = add_to_tree(obj, node->left);
      node->left->parent = node;
    } else {
      node->right = add_to_tree(obj, node->right);
      node->right->parent = node;
    }
    return node;
  }
};

} // namespace intrusive
