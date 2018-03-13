#include "MapBasedGlobalLockImpl.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h


bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {

    size_t new_element_size = key.size() + value.size();
    if (new_element_size > _max_size) {
        //too big
        return false;
    }

    //std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);
    if (find_element == _backend.end())
    {
        //element is absent
        return PutIfAbsent(key, value);

    }
    else {
        //if element is present
        return Set(key, value);
    }

}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {


    size_t new_element_size = key.size() + value.size();
    if (new_element_size > _max_size) {
        //too big
        return false;
    }

    std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);
    if (find_element != _backend.end()) {
        //already exists
        return false;
    }

    //clear memory for a new element by deleting least used
    size_t new_value_size = value.size();
    while((_current_size  + new_element_size) > _max_size) {
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

    //check if we have enought place
    size_t new_element_size = key.size() + value.size();
    if (new_element_size > _max_size) {
        //too big
        return false;
    }

    std::lock_guard<std::mutex> lock(_m);

    //check if the key is present
    auto find_element = _backend.find(key);
    if (find_element == _backend.end()) {
        //element is absent
        return false;
    }

    //clear space for a new
    size_t new_value_size = value.size();
    size_t old_value_size = find_element->second->value.size();
    while((_current_size - old_value_size + new_value_size) > _max_size) {
        if (DeleteTail() == false) {
            return false;
        }
    }

    //now set the  element
    find_element->second->value = value;
    _current_size = _current_size - old_value_size + new_value_size;

    //make head
    _values_list.make_head(find_element->second);
    return true;

}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {

    std::lock_guard<std::mutex> lock(_m);

    auto find_element = _backend.find(key);
    if (find_element == _backend.end()) {
        //element is absent
        return false;
    }

    auto node = find_element->second;
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

    //make head
    _values_list.make_head(find_element->second);

    return true;
}


bool MapBasedGlobalLockImpl::DeleteTail () {
    std::string key = _values_list.get_tail()->key;
    size_t element_size = key.size() + _values_list.get_tail()->value.size();
    _backend.erase(key);
    _values_list.pop_back();
    _current_size -= (element_size);
    return true;
}

} // namespace Backend
} // namespace Afina
