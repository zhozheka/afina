
#ifndef LIST_LRU_CPP
#define LIST_LRU_CPP


#include <iostream>
#include <functional>
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

private:
    size_t size;
    node<T>* head;
    node<T>* tail;

public:

    using reference = T&;
    using const_reference = const T&;


    list_lru()
        : size(0)
        , head(nullptr)
        , tail(nullptr) {}



    ~list_lru() {
        auto it = head;
        while(it != nullptr) {
            auto next_it = it->next;
            delete it;
            it = next_it;
        }
    }

    void print(const std::string &value) {
        auto it = head;
        int n = 0;
        cout << "------------" << value << "------------" << endl;
        cout << "head = " << head << endl;
        cout << "tail = " << tail << endl;

        while(it != nullptr) {
            cout << endl << "node " << n << "-------" << endl;
            cout << "   prev = " << it->prev << endl;
            cout << "   key = " << it->key << endl;
            cout << "   value = " << it->value << endl;
            cout << "   next = " << it->next << endl;
            it = it->next;
            n++;
        }
    }


    // void push_front(const T value) {
    //
    //     node<T>* new_node = new node<T>;
    //
    //     new_node->value = value;
    //     new_node->next = head;
    //     new_node->prev = nullptr;
    //
    //     if (size == 0) {
    //         tail = new_node;
    //         head = new_node;
    //     }
    //     else {
    //         head->prev = new_node;
    //     }
    //     head = new_node;
    //     size++;
    // }
    void push_front(const T value) {
        node<T>* new_node = new node<T>;

        new_node->value = value;

        if (head == nullptr) {
            new_node->prev = nullptr;
            new_node->next = nullptr;
            head = new_node;
            tail = new_node;
        }
        else {
            new_node->prev = nullptr;
            new_node->next = head;
            new_node->next->prev = new_node;
            head = new_node;
        }
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


    //
    // void make_head (node<T>* node) {
    //
    //     node->prev->next = node->next;
    //     if (node != tail)
    //     {
    //         node->next->prev = node->prev;
    //     }
    //
    //     node->prev = nullptr;
    //     node->next = head;
    //     head = node;
    // }

    void make_head (node<T>* node) {

        if (node == head) {
            return;
        }
        else if (node == tail) {
            //return;
            tail = node->prev;
            tail->next = nullptr;

            node->next = head;
            node->prev = nullptr;
            head->prev = node;
            head = node;

        }
        else {
            return;
            node->prev->next = node->next;
            node->next->prev = node->prev;

            node->prev = nullptr;
            node->next = head;
            head = node;
        }
    }

    node<T>* get_head() {
        return head;
    }

    node<T>* get_tail() {
        return tail;
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
    reference get_value(node<T>* node) {
        return node->value;
    }

    std::string get_key(node<T>* node) {
        return node->key;
    }


    void remove_tail() {
        remove(tail);
    }
};

#endif //LIST_LRU_CPP
