#ifndef AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H

#include <unordered_map>
#include <mutex>
#include <string>
#include <list>
#include <functional>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation with global lock
 *
 *
 */
 struct Node {
    Node* prev;
    Node* next;
    std::string first;
    std::string second;
 };

 class List {
 public:
    List();
    ~List();
    Node* front();
    Node* back();
    void pop_back();
    void push_front(std::string first, std::string second);
    void erase(Node* node);
    void to_front(Node* new_front);

 private:
    Node* _front;
    Node* _back;
 };

class MapBasedGlobalLockImpl : public Afina::Storage {
public:
    MapBasedGlobalLockImpl(size_t max_size = 1024) : _max_size(max_size), _size(0) {}
    ~MapBasedGlobalLockImpl() {}

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) const override;

private:
    size_t _max_size;
    size_t _size;
    mutable std::mutex _lock;

    std::unordered_map< std::reference_wrapper<const std::string>,
                        Node*,
                        std::hash<std::string>,
                        std::equal_to<std::string>> _backend;
    mutable List _cache;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
