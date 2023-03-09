#ifndef JOBQUEUE_H
#define JOBQUEUE_H

#include <QList>
#include <QMutex>

template <class T>
class JobQueue
{
public:
    explicit JobQueue() : _list(), _readMutex(), _writeMutex() {};

    void push(const T &data);
    T pop();
    int count();
    bool isEmpty();
    void clear();

private:
    QList<T> _list;
    QMutex _readMutex;
    QMutex _writeMutex;
};

template <class T>
void JobQueue<T>::push(const T &data)
{
    _writeMutex.lock();

    _list.push_back(data);

    _writeMutex.unlock();
}

template <class T>
T JobQueue<T>::pop()
{
    if (_list.isEmpty()) throw;

    _readMutex.lock();

    T data = _list.front();
    _list.pop_front();

    _readMutex.unlock();

    return data;
}

template <class T>
int JobQueue<T>::count()
{
    _readMutex.lock();

    int count = _list.count();

    _readMutex.unlock();

    return count;
}

template <class T>
bool JobQueue<T>::isEmpty()
{
    _readMutex.lock();

    bool isEmpty = _list.isEmpty();

    _readMutex.unlock();

    return isEmpty;
}

template <class T>
void JobQueue<T>::clear()
{
    _writeMutex.lock();

    _list.clear();

    _writeMutex.unlock();
}

#endif // JOBQUEUE_H
