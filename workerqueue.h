#ifndef WORKERQUEUE_H
#define WORKERQUEUE_H

#include <QList>
#include <QMutex>
#include <QDebug>

template <class T>
class WorkerQueue
{
public:
    explicit WorkerQueue(int maxSize) : _maxSize(maxSize), _list(), _readMutex(), _writeMutex() {};

    void push(const T &data);
    T pop();
    int count();
    bool isFull();
    void clear();

private:
    int _maxSize;
    QList<T> _list;
    QMutex _readMutex;
    QMutex _writeMutex;
};

template <class T>
void WorkerQueue<T>::push(const T &data)
{
    _writeMutex.lock();

    if (_list.count() == _maxSize)
    {
        _writeMutex.unlock();

        return;
    }

//    qDebug() << "push list.count():" << _list.count();

    _list.push_back(data);

    _writeMutex.unlock();
}

template <class T>
T WorkerQueue<T>::pop()
{
    if (_list.isEmpty()) throw;

    _readMutex.lock();

//    qDebug() << "pop list.count():" << _list.count();

    T data = _list.front();
    _list.pop_front();

    _readMutex.unlock();

    return data;
}

template <class T>
int WorkerQueue<T>::count()
{
    _readMutex.lock();

    int count = _list.count();

    _readMutex.unlock();

    return count;
}

template <class T>
bool WorkerQueue<T>::isFull()
{
    _readMutex.lock();

    bool isFull = _list.count() == _maxSize;

    _readMutex.unlock();

    return isFull;
}

template <class T>
void WorkerQueue<T>::clear()
{
    _writeMutex.lock();

    _list.clear();

    _writeMutex.unlock();
}

#endif // WORKERQUEUE_H
