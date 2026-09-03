#pragma once
#include<functional>
#include <iostream>
#include <cassert>

template<
    typename Key, 
    typename Value,
    typename Hash = std::hash<Key>
>
class HashMap {
    using value_type = std::pair<const Key, Value>;
    struct Node;

public : 
    
    class iterator {
        friend class HashMap;
        public : 

        iterator(const HashMap* map, std::size_t bucket_index, Node* node) :
            map_{map},
            bucket_index_{bucket_index},
            node_{node}
        {}

        value_type& operator*() const {
            assert(node_ != nullptr);
            return node_->value_;
        }

        value_type* operator->() const {
            assert(node_ != nullptr);
            return &node_->value_;
        }

        iterator& operator++() {
            if (node_->next_ != nullptr) {
                node_ = node_->next_;
                return *this;
            }
            while (++bucket_index_ != map_->buckets_.size()) {
                if (map_->buckets_[bucket_index_] != nullptr) {
                    node_ = map_->buckets_[bucket_index_];
                    return *this;
                }
            }
            node_ = nullptr;
            return *this; 
        }

        bool operator==(const iterator& other) const {
            return 
                map_ == other.map_ && 
                bucket_index_ == other.bucket_index_ && 
                node_ == other.node_;
        }

        private : 
        const HashMap* map_;
        std::size_t bucket_index_;
        Node* node_;
    };

    HashMap() :
        buckets_(initial_bucket_count_, nullptr),
        size_{0}
    {}

    HashMap(const HashMap& other) :
        buckets_{other.buckets_.size(), nullptr},
        size_{0}
    {
        for (const value_type& element : other) {
            insert(element);
        }
    }

    HashMap& operator=(const HashMap& other) {
        if (&other == this) {
            return *this;
        }
        free(buckets_);
        size_ = 0;
        for (const value_type& element : other) {
            insert(element);
        }
        return *this;
    }   

    HashMap(HashMap&& other) :
        HashMap() 
    {
        swap(other);
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (&other == this) {
            return *this;
        }
        swap(other);
        return *this;
    }

    bool empty() const {
        return size_ == 0;
    }

    std::size_t size() const {
        return size_;
    }

    iterator begin() const {
        for (std::size_t index = 0; index < buckets_.size(); ++index) {
            if (buckets_[index] != nullptr) {
                return iterator(this, index, buckets_[index]);
            }
        }
        return end();
    }

    iterator end() const {
        return iterator(this, buckets_.size(), nullptr);
    }

    iterator find(const Key& key) const {
        std::size_t index = bucket_index(key);
        Node* found_node = find_node(key, index);
        return found_node == nullptr ? end() : iterator(this, index, found_node);
    }

    Value& operator[](const Key& key) {
        std::size_t index = bucket_index(key);
        Node* found_node = find_node(key, index);
        if (found_node != nullptr) {
            return found_node->value_.second;
        }
        try_rehash(index, key);
        Node* new_node = new Node({key, Value()}, buckets_[index]);
        buckets_[index] = new_node;
        ++size_;
        return new_node->value_.second;
    }

    Value& operator[](Key&& key) {
        std::size_t index = bucket_index(key);
        Node* found_node = find_node(key, index);
        if (found_node != nullptr) {
            return found_node->value_.second;
        }
        try_rehash(index, key);
        Node* new_node = new Node({std::move(key), Value()}, buckets_[index]);
        buckets_[index] = new_node;
        ++size_;
        return new_node->value_.second;
    }

    iterator insert(const value_type& element) {
        const Key& key = element.first;
        const Value& value = element.second;
        std::size_t index = bucket_index(key);
        Node* found_node = find_node(key, index);
        if (found_node != nullptr) {
            found_node->value_.second = value;
            return iterator(this, index, found_node);
        }
        try_rehash(index, key);
        Node* new_node = new Node(element, buckets_[index]);
        buckets_[index] = new_node;
        ++size_;
        return iterator(this, index, new_node);
    }

    iterator insert(value_type&& element) {
        const Key& key = element.first;
        const Value& value = element.second;
        std::size_t index = bucket_index(key);
        Node* found_node = find_node(key, index);
        if (found_node != nullptr) {
            found_node->value_.second = value;
            return iterator{this, index, found_node};
        }
        try_rehash(index, key);
        Node* new_node = new Node(std::move(element), buckets_[index]);
        buckets_[index] = new_node;
        ++size_;
        return iterator{this, index, new_node};
    }

    std::size_t erase(const Key& key) {
        std::size_t index = bucket_index(key);
        if (buckets_[index] != nullptr && buckets_[index]->value_.first == key) {
            Node* erased_node = buckets_[index];
            buckets_[index] = buckets_[index]->next_;
            --size_;
            delete erased_node;
            return 1;
        }
        for (Node* cur = buckets_[index]; cur != nullptr; cur = cur->next_) {
            if (cur->next_ != nullptr && cur->next_->value_.first == key) {
                Node* erased_node = cur->next_;
                cur->next_ = cur->next_->next_;
                delete erased_node;
                --size_;
                return 1;   
            }
        }
        return 0;
    }

    iterator erase(const iterator& it) {
        assert(it != end());
        std::size_t index = it.bucket_index_;
        if (buckets_[index] != nullptr && buckets_[index] == it.node_) {
            iterator next = it; ++next;
            Node* erased_node = buckets_[index];
            buckets_[index] = buckets_[index]->next_;
            delete erased_node;
            --size_;
            return next;
        }
        for (Node* cur = buckets_[index]; cur != nullptr; cur = cur->next_) {
            if (cur->next_ == it.node_) {
                cur->next_ = cur->next_->next_;
                iterator next = it; ++next;
                --size_;
                delete it.node_;
                return next;
            }
        }
        return end();
    }

    void rehash(std::size_t new_bucket_count) {
        std::vector<Node*> old_bucket{new_bucket_count, nullptr};
        std::swap(buckets_, old_bucket);
        size_ = 0;
        for (Node* bucket : old_bucket) {
            for (Node* cur = bucket; cur != nullptr; ) {
                Node* next = cur->next_;
                link_node(cur);
                cur = next;
            }
        }
    }

    ~HashMap() {
        free(buckets_);
    }

private : 
    struct Node {
        value_type value_;
        Node* next_ = nullptr;
    };

    std::size_t bucket_index(const Key& key) const {
        return hasher_(key) % buckets_.size();
    }

    Node* find_node(const Key& key, std::size_t bucket_index) const {
        for (Node* cur = buckets_[bucket_index]; cur != nullptr; cur = cur->next_) {
            if (cur->value_.first == key) {
                return cur;
            }
        }
        return nullptr;
    }

    void link_node(Node* node) {
        const Key& key = node->value_.first;
        std::size_t index = bucket_index(key);
        node->next_ = buckets_[index];
        buckets_[index] = node;
        ++size_;
    }

    void try_rehash(std::size_t& index, const Key& key) {
        if (size_ + 1 > buckets_.size() * max_load_factor_) {
            rehash(buckets_.size() * growth_factor_);
            index = bucket_index(key);
        }
    }

    void free(std::vector<Node*>& buckets) {
        for (std::size_t i = 0; i < buckets.size(); ++i) {
            for (Node* cur = buckets[i]; cur != nullptr;) {
                Node* next = cur->next_;
                delete cur;
                cur = next;
            }
            buckets[i] = nullptr;
        }
    }

    void swap(HashMap& other) {
        std::swap(buckets_, other.buckets_);
        std::swap(size_, other.size_);
        std::swap(hasher_, other.hasher_);
    }

    static constexpr size_t growth_factor_ = 2;
    static constexpr size_t initial_bucket_count_ = 8;
    static constexpr float max_load_factor_ = 1.7f;

    std::vector<Node*> buckets_;
    std::size_t size_;
    Hash hasher_{};
};  