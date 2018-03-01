//
// Created by Evgeny Zholkovskiy on 21/02/2018.
//

#ifndef TEST1_LIST_H
#define TEST1_LIST_H


#include <iostream>
using namespace std;

template <class T>
struct node
{
    T value;
    std::string key;
    node<T>* next;
    node<T>* prev;
};

template <class T>
class list_lru
{

public:
    size_t size;
    node<T>* head;
    node<T>* tail;

public:

    using reference = T&;
    using const_reference = const T&;

    list_lru() {
        list_lru::size = 0;
        list_lru::head = nullptr;
        list_lru::tail = nullptr;
    }


    ~list_lru() {
        auto it = head;
        while(it != nullptr) {
            auto next_it = it->next;
            delete it;
            it = next_it;
        }
    }


    void push_back(const T value, const std::string &key="") {

        node<T>* new_node = new node<T>;

        new_node->value = value;
        new_node->key = key;
        new_node->next = nullptr;
        new_node->prev = tail;


        if (size == 0) {
            head = new_node;
            tail = new_node;
        }
        else {
            tail->next = new_node;
        }
        tail = new_node;
        size++;
    }


    void push_front(const T value, const std::string &key="") {

        node<T>* new_node = new node<T>;

        new_node->value = value;
        new_node->key = key;
        new_node->next = head;
        new_node->prev = nullptr;

        if (size == 0) {
            tail = new_node;
            head = new_node;
        }
        else {
            head->prev = new_node;
        }
        head = new_node;
        size++;
    }


    void pop_back() {

        if (size == 0) {
        }
        else if (size == 1) {

            delete head;
            head = nullptr;
            tail = nullptr;
            size = 0;
        }
        else {
            auto new_tail = tail->prev;
            delete tail;
            tail = new_tail;
            tail->next = nullptr;
            size--;
        }
    }


    void pop_front() {

        if (size == 0) {
        }
        else if (size == 1) {

            delete head;
            head = nullptr;
            tail = nullptr;
            size = 0;
        }
        else {
            auto new_head = head->next;
            delete head;
            head = new_head;
            head->prev = nullptr;
            size--;
        }
    }

    void make_head(size_t num) {

        if (num == 0) {
            return;
        }


        node<T>* node = node_n(num);
        node->prev->next = node->next;
        if (num != size-1) {
            node->next->prev = node->prev;
        }

        node->prev = nullptr;
        node->next = head;
        head = node;
    }


    node<T>* get_head() {
        return head;
    }

    node<T>* get_tail() {
        return tail;
    }

    void remove(size_t num) {
        auto node = node_n(num);
        remove(node);
    }

    void remove(node<T>* node) {

        if ((node == head) && (node == tail)) {
            tail = nullptr;
            head = nullptr;
            delete node;
            size = 0;
        }
        else if (node == tail) {
            node->prev->next = nullptr;
            tail = node->prev;
            delete node;
            size--;
        }
        else if (node == head) {
            node->next->prev = nullptr;
            head = node->next;
            delete node;
            size--;
        }
        else {
            node->next->prev = node->prev;
            node->prev->next = node->next;
            delete node;
            size--;
        }
    }
    node<T>* node_n(size_t num) {

        if ((num<0) || (num>=size)) {
            cout << "error" << endl;
        }
        auto node = head;
        for (int i=0; i<num; i++) {
            node = node->next;
        }
        return node;
    }


    reference operator[](size_t num)
    {
        auto node = node_n(num);
        return node->value;
    }

    reference get_value(size_t num) {
        auto node = node_n(num);
        return node->value;
    }

    reference get_value(node<T>* node) {
        return node->value;
    }


    std::string get_key(size_t num)
    {
        auto node = node_n(num);
        return node->key;
    }
    std::string get_key(node<T>* node) {
        return node->key;
    }


    void remove_tail() {
        remove(tail);
    }
    void print() {

        auto it = head;
        while (it != nullptr) {
            std::cout << "[" << it->value << ", " << it->key <<"], ";
            it = it->next;
        }
        std::cout << std::endl;

    }
};

#endif //TEST1_LIST_H
