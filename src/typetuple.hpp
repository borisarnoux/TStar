#include <stdio.h>
#include <stdint.h>
#include <delegator.hpp>

#define _round_toptrsize(val) ((val)%sizeof(intptr_t)==0?(val):(val)-((val)%sizeof(intptr_t)) + sizeof(intptr_t))


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
	int & dyntype = *(int*)get_pos(pos);
	dyntype = layout_type_to_data::get_dyn_type( (T*) NULL );
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

/**** Named Locator *****/



template <int pos, typename ...A>
struct named_locator_helper;

template <int pos, typename N, typename T>
struct named_locator_helper<pos,N, T> {

  char buffer[pos+_round_toptrsize(sizeof(coords))];

  template <typename R>
  inline frame_struct * & get_framep( N d ) {
    return (*( coords*) get_pos(pos)).page;
  }

  template <typename R>
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
  template <typename R>
  inline frame_struct * & get_framep( N d ) {
    return (*( coords*) get_pos(pos)).page;
  }

  using named_locator_helper<pos+_round_toptrsize(sizeof(coords)), A...>::get_offset;
  template <typename R>
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




#define vdef(name) struct name##__innervar *
#define vname(name) ((struct name##__innervar *)NULL)

template <typename T1, typename T2>
struct fusion : public T1, public T2 {
  fusion() : T1(), T2() {
	printf( "Fusion constructor.\n");  
  }

  static const int length = T1::length + T2::length;
};


template <typename T1, typename T2>
struct static_task_data {
    static_task_data() {

    }

    void (*fn)();
    int nargs;
    static const int length = T1::length + T2::length;
    typedef fusion<typename T1::layout, typename T2::layout> layout_type;

    union {
        fusion<typename T1::layout, typename T2::layout> layout;
        intptr_t elements[layout_type::length];
    };

};

template <typename T1,typename T2, typename L>
struct task_data : public T1, public T2 {
   task_data() : T1(), T2() {
		printf( "Taskdata constructor : %u %u\n", sizeof(T1), sizeof(T2) );
                static_data_p = &layout;
                layout.fn = &exec_lambda;
    }

    L lambda;

    static void exec_lambda() {
        struct task_data<T1,T2,L> * t = (struct task_data<T1,T2,L> *) tstar_getcfp();
        t->lambda();
    }

    struct static_task_data<T1,T2> * static_data_p;
    static static_task_data<T1,T2> layout;

    typedef T1 needs;
    typedef T2 provides;

    using needs::get;
    using needs::get_byname;
    using provides::get_offset;
};
template <typename T1,typename T2,typename L>
static_task_data<T1,T2> task_data<T1,T2,L>::layout;



/*
#define bind( T1, var1, T2, var2 ) do {
  size_t & offset =  T1.get_offset(vname(var1));
  struct frame_struct * &target  = T1.get_framep(vname(var1));

  offset = T2.get_byname( vname(var1) );
  

} while (0) */

int main() {
    task_data<named_vector<vdef(Cat),int>,named_locator<vdef(Mouse),int,vdef(Mom), int>, >a;
    
  a.needs::get<int,0>() = 3;
  size_t & off = a.get_offset<size_t>( vname(Mom) );
  off = 4;

  auto z = a.layout;
  int * zp = (int*) &z;
  for ( int i = 0; i < z.length ; ++i ) {
	printf( "%d\n", zp[i]) ;
  }

  printf( "%u %u %d\n", sizeof( a ), sizeof( z ), a.get_offset<int>(vname(Mom)) );

}



