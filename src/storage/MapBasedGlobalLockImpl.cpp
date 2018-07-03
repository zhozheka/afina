#include "MapBasedGlobalLockImpl.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_lock);

    size_t needed_size = key.size() + value.size();
    if (needed_size > _max_size)
    {
        return false;
    }

    auto cache_elem = _backend.find(key);
    if (cache_elem != _backend.end())
    {
        _cache.to_front(cache_elem->second);
        _cache.front()->second = value;
        return true;
    }

    while (needed_size > _max_size - _size)
    {
        auto old_key = _cache.back();
        _size -= old_key->first.size();
        _size -= old_key->second.size();
        _cache.pop_back();
        _backend.erase(old_key->first);
    }
    _size += needed_size;
    _cache.push_front(key, value);
    _backend[_cache.front()->first] = _cache.front();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_lock);

    const size_t needed_size = key.size() + value.size();
    if (needed_size > _max_size)
    {
        return false;
    }

    if (_backend.find(key) == _backend.end())
    {
        while (needed_size > _max_size - _size)
        {
            auto old_key = _cache.back();
            _size -= old_key->first.size();
            _size -= old_key->second.size();
            _cache.pop_back();
            _backend.erase(old_key->first);
        }
        _size += needed_size;
        _cache.push_front(key, value);
        _backend[_cache.front()->first] = _cache.front();
        return true;
    }

    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(_lock);

    size_t needed_size = value.size();
    if (needed_size > _max_size)
    {
        return false;
    }

    auto cache_elem = _backend.find(key);
    if (cache_elem != _backend.end())
    {
        _cache.to_front(cache_elem->second);
        needed_size -= _cache.front()->second.size();
        while (needed_size > _max_size - _size)
        {
            auto old_key = _cache.back();
            _size -= old_key->first.size();
            _size -= old_key->second.size();
            _cache.pop_back();
            _backend.erase(old_key->first);
        }
        _cache.front()->second = value;
        return true;
    }

    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key)
{
    std::lock_guard<std::mutex> lock(_lock);

    auto cache_elem = _backend.find(key);
    _cache.erase(cache_elem->second);
    _backend.erase(key);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const
{
    std::lock_guard<std::mutex> lock(_lock);
    
    auto cache_it = _backend.find(key);
    if (cache_it != _backend.end())
    {
        _cache.to_front(cache_it->second);
        value = _cache.front()->second;
        return true;
    }

    return false;
}

List::List()
{
    _front = NULL;
    _back = NULL;
}

List::~List()
{
    if (_front == NULL)
    {
        return;
    }
    while (_front->next != NULL)
    {
        _front = _front->next;
        delete _front->prev;
    }
    delete _front;
}

Node* List::front()
{
    return _front;
}

Node* List::back()
{
    return _back;
}

void List::pop_back()
{
    if (_back->prev != NULL)
    {
        _back = _back->prev;
        delete _back->next;
        _back->next = NULL;
    } else {
        _front = NULL;
        delete _back;
        _back = NULL;
    }
}

void List::erase(Node* node)
{
    if (node->next == NULL)
    {
        pop_back();
    }
    if (node->prev == NULL)
    {
        _front = node->next;
        _front->prev = NULL;
    } else {
        node->prev->next = node->next;
    }
    node->next->prev = node->prev;
    delete node;
}

void List::push_front(std::string first, std::string second)
{
    Node* tmp = new Node;
    tmp->next = _front;
    tmp->prev = NULL;
    tmp->first = first;
    tmp->second = second;
    if (_front != NULL)
    {
        _front->prev = tmp;
    }
    _front = tmp;
    if (_back == NULL)
    {
        _back = _front;
    }
}

void List::to_front(Node* new_front)
{
    if (new_front->prev == NULL)
    {
        return;
    }
    new_front->prev->next = new_front->next;
    if (new_front->next != NULL)
    {
        new_front->next->prev = new_front->prev;
    } else if ((_back != NULL) && (_back->prev != NULL)) {
        _back->prev->next = NULL;
    }
    new_front->prev = NULL;
    new_front->next = _front;
    if (_front != NULL)
    {
        _front->prev = new_front;
    }
    _front = new_front;
}

} // namespace Backend
} // namespace Afina
