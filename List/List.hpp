#pragma once
#include<cstddef>
#include<utility>
#include<cassert>
#include <iostream>

template<typename T>
class List {
private : 
    struct Node;
public :
    class iterator {
        public : 
            iterator(Node* node) : node_{node} {}

            T& operator*() const {
                return node_->value_;
            }

            iterator& operator++() {
                node_ = node_->next_;
                return *this;
            }

            bool operator==(const iterator& other) const {
                return node_ == other.node_;
            }
        private : 
            friend class List;
            Node* node_;
    };

    List() :
        dummy_{new Node(nullptr, nullptr, T())},
        begin_node_{dummy_},
        end_node_{dummy_},
        size_{0}
    {
    }

    List(const List& other) :
        List()
    {
        
        for (const auto& x : other) {
            push_back(x);
        }
    }

    List& operator=(const List& other) 
    {   
        if (this == &other) {
            return *this;
        }
        reset();
        for (const auto& x : other) {
            push_back(x);
        }
        return *this;
    }

    List(List&& other) noexcept :
        List()
     {
        swap(other);
        
    }

    List& operator=(List&& other) 
    noexcept    {
        if (this == &other) {
            return *this;
        }
        reset();
        swap(other);
        return *this;
    }

    template<typename Container>
    List(const Container& container) :
        List()
    {
        for (const auto& element : container) {
            push_back(element);
        }
    }

    List(std::initializer_list<T> values) :
        List()
    {
        for(const T& value : values) {
            push_back(value);
        }
    }

    iterator begin() const {
        return iterator{begin_node_};
    }

    iterator end() const {
        return iterator{end_node_};
    }

    bool empty() const {
        return size_ == 0;
    }

    std::size_t size() const {
        return size_;
    }

    T& front() const {
        return begin_node_->value_;
    }

    T& back() const {
        return end_node_->previous_->value_;
    }

    iterator insert(const iterator& pos, const T& element) {
        Node* current_node = pos.node_;
        Node* new_node = new Node(current_node->previous_, current_node, element);
        if (new_node->previous_ != nullptr) {
            new_node->previous_->next_ = new_node;
        } else {
            begin_node_ = new_node;
        }
        assert(new_node->next_ == current_node);
        new_node->next_->previous_ = new_node;
        ++size_;
        return iterator{new_node};
    }

    iterator insert(const iterator& pos, T&& element) {
        Node* current_node = pos.node_;
        Node* new_node = new Node(current_node->previous_, current_node, std::move(element));
        if (new_node->previous_ != nullptr) {
            new_node->previous_->next_ = new_node;
        } else {
            begin_node_ = new_node;
        }
        assert(new_node->next_ == current_node);
        new_node->next_->previous_ = new_node;
        ++size_;
        return iterator{new_node};
    }

    void push_front(const T& element) {
        insert(begin(), element);
    }

    void push_front(T&& element) {
        insert(begin(), std::move(element));
    }

    void push_back(const T& element) {
        insert(end(), element);
    }

    void push_back(T&& element) {
        insert(end(), std::move(element));
    }

    iterator erase(const iterator& pos) {
        assert(pos != end());
        Node* erased_node = pos.node_;

        if (erased_node->previous_ != nullptr) {
            erased_node->previous_->next_ = erased_node->next_;
        } else {
            assert(erased_node == begin_node_);
            begin_node_ = erased_node->next_;
        }

        erased_node->next_->previous_ = erased_node->previous_;
        Node* tmp = erased_node->next_;
        delete erased_node;
        --size_;
        return iterator{tmp};
    }

    ~List() {
        reset();
        delete dummy_;
    }

private :
    struct Node {
        Node* previous_;
        Node* next_;
        T value_;
    };

    void reset() {
        Node* cur = begin_node_;
        while (cur != end_node_) {
            Node* next = cur->next_;
            delete cur;
            cur = next;
        }
        begin_node_ = end_node_;
        dummy_->previous_ = dummy_->next_ = nullptr;
        size_ = 0;
    }

    void swap(List& other) {
        std::swap(begin_node_, other.begin_node_);
        std::swap(dummy_, other.dummy_);
        std::swap(end_node_, other.end_node_);
        std::swap(size_, other.size_);
    }

    Node* dummy_;
    Node* begin_node_;
    Node* end_node_;
    std::size_t size_;

    
};