#ifndef AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
#define AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include "list_lru.h"
#include <afina/Storage.h>
#include <functional>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation with global lock
 *
 *
 */
class MapBasedGlobalLockImpl : public Afina::Storage {
public:
    MapBasedGlobalLockImpl(size_t max_size = 1024)
    : _max_size(max_size)
    , _current_size(0) {}
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

    // Deletes the last element of _backend
    bool DeleteTail();



private:
    using value_type = std::string;

    mutable list_lru<value_type> _values_list; //pair of key and value

    using backend_wrapper  =  std::unordered_map <std::reference_wrapper<const std::string>,
    node<std::string>*,
    std::hash<std::string>,
    std::equal_to<std::string>>;

    mutable backend_wrapper _backend;


    size_t _max_size;
    size_t _current_size;

    mutable std::mutex _m;

};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_MAP_BASED_GLOBAL_LOCK_IMPL_H
