#include <stdio.h>
#include <stdint.h>

#define _round_toptrsize(val) ((val)%sizeof(intptr_t)==0?(val):(val)-((val)%sizeof(intptr_t)) + sizeof(intptr_t))

#define CONCAT(A,B) A##B

struct coords {
    struct frame_struct * page;
    size_t offset;
};

/**** Layout Descriptor Mapping *****/

struct layout_type_to_data {
  static int get_dyn_type( struct fat_pointer_buffer * n ) {
	return 3;
  }

  static int get_dyn_type( struct wo_frame * n ) {
	return 4;
  }

  static int get_dyn_type( struct ro_frame * n ) {
	return 5;
  }

  static int get_dyn_type( struct rw_frame * n ) {
	return 6;
  }

  template <typename T>
  static int get_dyn_type( T* a ) {
	return 7;
  }
};


/***** Layout descriptor for locator *****/
template <int pos, typename ...A>
struct locator_layout_descriptor_helper;

template <int pos, typename N, typename T>
struct locator_layout_descriptor_helper<pos,N, T> {
  char buffer[pos+_round_toptrsize(sizeof(coords))];

  locator_layout_descriptor_helper() {
	int * dtp = (int*)get_pos(pos);
	dtp[0] = layout_type_to_data::get_dyn_type( (struct wo_frame *) NULL );
	dtp[1] = layout_type_to_data::get_dyn_type( (char *) NULL );
	printf( "Layout set elt at pos %d : %d, and %d\n", pos, dtp[0],dtp[1] );
  }


  inline void * get_pos( int rpos ) {
	return buffer + rpos;
  }
};

template <int pos, typename N, typename T, typename ...A>
struct locator_layout_descriptor_helper<pos, N,T, A...> {
  locator_layout_descriptor_helper<pos+_round_toptrsize(sizeof(coords)), A...> inner;
  
  locator_layout_descriptor_helper() : inner() {
        int * dyntype = (int*)get_pos(pos);
        dyntype[0] = layout_type_to_data::get_dyn_type( (struct wo_frame *) NULL  );
        dyntype[1] = layout_type_to_data::get_dyn_type( (char *) NULL );
	printf( "Layout set elt at pos %d : %d\n", pos, dyntype );
  }


  inline void * get_pos( int rpos ) {
  	return inner.get_pos(rpos);
  }
};

template <typename ...A>
struct locator_layout_descriptor : public locator_layout_descriptor_helper<0, A... >  {
  locator_layout_descriptor() : locator_layout_descriptor_helper<0,A...>()  {
  } 
  static const int length = sizeof...(A);
};

template <int pos, typename ...A>
struct layout_descriptor_helper;

template <int pos, typename N, typename T>
struct layout_descriptor_helper<pos,N, T> {
  char buffer[pos+_round_toptrsize(sizeof(T))];

  layout_descriptor_helper() {
	int & dyntype = *(int*)get_pos(pos);
	dyntype = layout_type_to_data::get_dyn_type( (T*) NULL );
	printf( "Layout set elt at pos %d : %d\n", pos, dyntype );
  }


  inline void * get_pos( int rpos ) {
	return buffer + rpos;
  }
};

template <int pos, typename N, typename T, typename ...A>
struct layout_descriptor_helper<pos, N,T, A...> {
  layout_descriptor_helper<pos+_round_toptrsize(sizeof(T)), A...> inner;
  
  layout_descriptor_helper() : inner() {
	int & dyntype = *(int*)get_pos(pos);
	dyntype = layout_type_to_data::get_dyn_type( (T*) NULL );
	printf( "Layout set elt at pos %d : %d\n", pos, dyntype );
  }


  inline void * get_pos( int rpos ) {
  	return inner.get_pos(rpos);
  }
};

template <typename ...A>
struct layout_descriptor : public layout_descriptor_helper<0, A... >  {
  layout_descriptor() : layout_descriptor_helper<0,A...>()  {
  } 
  static const int length = sizeof...(A)/2;
};

template <>
struct layout_descriptor<> {
    static const int length = 0;
};

/* Named Vector Typechecker (only for sequential get accesses) */

template <int rank, typename...A>
struct named_vector_typechecker;

template <int rank, typename N, typename T, typename ...A>
struct named_vector_typechecker<rank, N,T, A...> : public named_vector_typechecker<rank-1, A...> {
};

template <typename N, typename T, typename ...A>
struct named_vector_typechecker<0,N,T,A...> {
	typedef T head;
};


template <int pos, typename ...A>
struct named_vector_helper;

template <int pos, typename N, typename T>
struct named_vector_helper<pos,N, T> {
  char buffer[pos+_round_toptrsize(sizeof(T))];

  template <typename R>
  inline R & get_byname( N d ) {
    return *( T*) get_pos(pos);
  }

  template<typename R, int rank>
  inline R & get() {
	static_assert(rank==0, "Invalid rank in task struct access");
	void * ptr = get_rec( rank );
	return *( typename named_vector_typechecker<rank,N,T>:: head *)ptr;
  }

  inline void * get_rec( int rank ) {
	if ( rank == 0 ) {
	  return get_pos(pos);
	} else {
	  // error ( type checked before )
	  return NULL;
	}
  }

  inline void * get_pos( int rpos ) {
	return buffer + rpos;
  }
};

template <int pos, typename N, typename T, typename ...A>
struct named_vector_helper<pos, N,T, A...> : public named_vector_helper<pos+_round_toptrsize(sizeof(T)), A...> {


  using named_vector_helper<pos+_round_toptrsize(sizeof(T)), A...>::get_byname;
  template <typename R>
  inline R & get_byname( N d ) {
    return *( T*) get_pos(pos);
  }

  template<typename R, int rank>
  inline R & get() {
	static_assert(rank < _round_toptrsize(sizeof...(A)+1), "task struct access out of bounds");
	void * ptr = get_rec( rank );
	return *( typename named_vector_typechecker<rank,N,T,A...>:: head *)ptr;
  }

  inline void * get_rec( int rank ) {
	if ( rank == 0 ) {
	  return get_pos( pos );
	} else {
	  return named_vector_helper<pos+_round_toptrsize(sizeof(T)), A...>::get_rec(rank-1 );
	}
  }

  inline void * get_pos( int rpos ) {
  	return named_vector_helper<pos+_round_toptrsize(sizeof(T)), A...>::get_pos(rpos);
  }
};



template <typename ...A>
struct named_vector : public named_vector_helper<0, A... >  {
  typedef layout_descriptor<A...> layout;
  static const int length = sizeof...(A)/2;

};

template <>
struct named_vector<> {
    typedef layout_descriptor<> layout;
    static const int length = 0;
};

/**** Named Locator *****/



template <int pos, typename ...A>
struct named_locator_helper;

template <int pos, typename N, typename T>
struct named_locator_helper<pos,N, T> {

  char buffer[pos+_round_toptrsize(sizeof(coords))];

  inline frame_struct * & get_framep( N d ) {
    return (*( coords*) get_pos(pos)).page;
  }

  inline size_t & get_offset( N d ) {
    return (*( coords*) get_pos(pos)).offset;
  }


  inline void * get_pos( int rpos ) {
	return buffer + rpos;
  }
};

template <int pos, typename N, typename T, typename ...A>
struct named_locator_helper<pos, N,T, A...> : public named_locator_helper<pos+_round_toptrsize(sizeof(coords)), A...> {


    using named_locator_helper<pos+_round_toptrsize(sizeof(coords)), A...>::get_framep;
    using named_locator_helper<pos+_round_toptrsize(sizeof(coords)), A...>::get_offset;

  inline frame_struct * & get_framep( N d ) {
    return (*( coords*) get_pos(pos)).page;
  }


  inline size_t & get_offset( N d ) {
    return (*( coords*) get_pos(pos)).offset;
  }


  inline void * get_pos( int rpos ) {
  	return named_locator_helper<pos+_round_toptrsize(sizeof(coords)), A...>::get_pos(rpos);
  }
};

template <typename ...A>
struct named_locator : public named_locator_helper<0, A... >  {

        typedef locator_layout_descriptor<A...> layout;
	static const int length = sizeof...(A)/2;
};





template <typename T1, typename T2>
struct fusion : public T1, public T2 {
  fusion() : T1(), T2() {
	printf( "Fusion constructor.\n");  
  }

  static const int length = T1::length + T2::length;
};


template <typename T1, typename T2>
struct static_task_data {
    static_task_data() : layout() {

    }

    void (*fn)();
    int nargs;
    typedef fusion<typename T1::layout, typename T2::layout> layout_type;

    union {
        fusion<typename T1::layout, typename T2::layout> layout;
        intptr_t elements[layout_type::length];
    };

    static const int length = T1::length + T2::length;

};

template <typename T1,typename T2>
struct task_data_nocontext {
    task_data_nocontext( int _sc ) : sc(_sc) {
        printf( "Taskdata_nocontext sc=%d (at %d) constructor : %u %u\n", sc, (intptr_t)&sc-(intptr_t)this,
                                sizeof(T1), sizeof(T2) );
                static_data_p = &layout;
    }


    struct static_task_data<T1,T2> * static_data_p;
    int sc;
    union {
        struct {
            T1 needs;
            T2 provides;
        };
        void * args[static_task_data<T1,T2>::length];
    };


    static static_task_data<T1,T2> layout;

    template<typename R, int rank>
    inline R & get() {
        return needs.get<R,rank>();
    }
    template<typename R, typename T>
    inline R & get_byname( T m) {
        return needs.get_byname<R>( m );
    }

    template <typename N>
    inline frame_struct * & get_framep( N d ) {
        return provides.get_framep( d );
    }

    template <typename N>
    inline size_t & get_offset( N d ) {
        return provides.get_offset( d );
    }

};

template <typename T1,typename T2>
static_task_data<T1,T2> task_data_nocontext<T1,T2>::layout;


template <typename T1, typename L>
struct task_data : public T1 {
    task_data(int sc, L _lambda) : T1(sc), lambda(_lambda) {
                printf( "Taskdata with context :" " %u\n", sizeof(L) );
                T1::layout.fn = &exec_lambda;
    }


    static void exec_lambda() {
        struct task_data<T1,L> * t = (struct task_data<T1,L> *) tstar_getcfp();
        t->lambda();
    }



    L lambda;


};



template <typename T1, typename L>
struct task_data<T1,L> * _create_task(int sc, L lambda)  {
    return new task_data<T1,L>(sc, lambda);
}



template  <typename CT>
struct type_provider {
    //typedef task_data_nocontext<T1,T2> cfptype;
    typedef CT cfptype;
    template <typename R, typename T>
    static inline R& _get_arg( T d ) {
        return ((cfptype*)tstar_getcfp())->template get_byname<R>(d);
    }

    template <typename N>
    inline frame_struct * & _get_framep( N d ) {
        return ((cfptype*)tstar_getcfp())->template get_framep( d );
    }

    template <typename N>
    inline size_t & _get_offset( N d ) {
        return ((cfptype*)tstar_getcfp())->template get_offset( d );
    }
};

#define vdef(name) struct name##__innervar *
#define vname(name) ((struct name##__innervar *)NULL)

#define get_arg(name, type ) tprovider._get_arg<type>(vname(name))
#define get_off(name) tprovider._get_offset(vname(name))
#define get_fra(name) tprovider._get_framep(vname(name))

#define set_arg(task_hdl, name, type, val)\
do {\
task_hdl->get_byname<type>(vname(name)) = val;        \
} while (0);

#define _input(...) named_vector<__VA_ARGS__>
#define _output(...) named_locator<__VA_ARGS__>
#define _code(...) __VA_ARGS__

#define THANDLE(T1,T2) task_data_nocontext<T1,T2>

#define TASK(sc, taskhandle, BLOCK) \
 ([=] {\
    return _create_task<taskhandle>( sc, [=]{\
            type_provider<taskhandle> tprovider;\
            BLOCK\
            }\
        );\
    } )()\




#define bind( T1, var1, T2, var2, type ) \
    do {\
  size_t & offset =  T1->get_offset(vname(var1));\
  struct frame_struct * &target  = T1->get_framep(vname(var1));\
  offset = T2->get_byname<type>( vname(var2) );\
} while (0)

#define bind_outout(T1, var1, nameloc )\
 do {\
    size_t & offset =  T1->get_offset(vname(var1));\
    struct frame_struct * &target  = T1->get_framep(vname(var1));\
    offset = get_off( nameloc );\
    target = get_fra( nameloc );\
 } while ( 0 )


#define provide( name, type, value )\
     do {\
    type &ref = *(type*) ((intptr_t)get_fra(name)+(intptr_t) get_off(name) );\
    ref = value;\
    tstar_tdec( get_fra(name), NULL );\
    } while(0)\


