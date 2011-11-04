
#ifndef NODE_H
#define NODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef  int node_id_t;

extern node_id_t my_node;

inline node_id_t get_node_num() {
	return my_node;
}

#ifdef __cplusplus
}
#endif

#endif


