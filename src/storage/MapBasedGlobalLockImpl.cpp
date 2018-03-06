#include "MapBasedGlobalLockImpl.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h


bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {

    std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);
    if (find_element != _backend.end())
    {
        if (Delete(key) == false) { //delete old
            return false;
        }
    }
    return PutIfAbsent(key, value);

}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {

    std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);

    if (find_element != _backend.end()) {
        //already exists
        return false;
    }

    size_t new_element_size = key.size() + value.size();
    if (new_element_size > _max_size) {
        //too big
        return false;
    }

    //clear memory for a new element by deleting least used
    while((_current_size + new_element_size) > _max_size) {
        if (DeleteTail() == false) {
            return false;
        }
    }
    //put new element
    _values_list.push_front(value);
    _values_list.get_head()->key = key; //
    _backend[_values_list.get_head()->key] = _values_list.get_head();
    _current_size += new_element_size;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {

    std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);
    if (find_element == _backend.end()) {
        //element is absent
        return false;
    }
    //delete existing
    if (Delete(key) == false) {
        return false;
    }

    return PutIfAbsent(key, value);
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {

    std::lock_guard<std::mutex> lock(_m);

    auto element = _backend.find(key);
    if (element == _backend.end()) {
        //element is absent
        return false;
    }

    auto node = element->second;
    size_t element_size = key.size() + node->value.size(); //get size
    _backend.erase(key); //delete from map
    _values_list.remove(node); //delete from list

    _current_size -= element_size; //correct size
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {

    std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);
    if (find_element == _backend.end()) {
        //element is not present
        return false;
    }

    value = find_element->second->value;
    return true;

}

bool MapBasedGlobalLockImpl::DeleteTail() {

    std::lock_guard<std::mutex> lock(_m);
    
    auto tail_key = _values_list.get_tail()->key;
    return Delete(tail_key);
}

} // namespace Backend
} // namespace Afina
