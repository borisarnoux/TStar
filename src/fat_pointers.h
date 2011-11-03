




// A fat pointer is a recursive object description for acquiring.
// For garbage collection purposes, it shall benefit from a special handling



struct fat_pointer_header {
  // This use count is a problem : we propose simply using a tdec.
  int sc;
  int count;

  struct fp_node fp_nodes[];
}


struct fp_node {
  int type; // Type : Leaf or branch.
  int perms; // Perms for acquiring.
  void * frame;
  void * base;
  size_t size;
}
