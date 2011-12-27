#ifndef TSTAR_MPSC_H
#define TSTAR_MPSC_H


// (c) notice : algorithm from 1024cores.net

#define XCHG(ptr,val) __sync_lock_test_and_set ((ptr), (val))

template <typename T>
struct MPSC;

template<typename T>
struct MPSC<T*>
{

    struct node_t
    {
        node_t* volatile  next;
        T* value;

    };


    node_t* volatile  head;
    node_t*           tail;
    node_t            stub;

    MPSC() {
        head = &stub;
        tail = &stub;
        stub.next = 0;
    }





    void push(T* _v)
    {
        node_t * n = new node_t;
        n->value = _v;
        push( n );

    }

    void push(node_t * n) {
        n->next = 0;
        node_t* prev = XCHG(&this->head, n);
        //(*)
        prev->next = n;

    }

    T* pop()
    {
        node_t* tail = this->tail;
        node_t* next = tail->next;
        if (tail == &this->stub)
        {
            if (0 == next)
                return (T*)0;
            this->tail = next;
            tail = next;
            next = next->next;
        }
        if (next)
        {
            this->tail = next;

            T* retval = tail->value;
            delete tail;
            return retval;
        }
        node_t* head = this->head;

        if (tail != head)
            return (T*)0;
        push( &this->stub);
        next = tail->next;
        if (next)
        {
            this->tail = next;

            T* retval = tail->value;
            delete tail;
            return retval;
        }
        return (T*)0;
    }

};

#endif // TSTAR_MPSC_H
