#ifndef RAYDOM_TREE_H
#define RAYDOM_TREE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rd_ctx rd_ctx;

// Generational handle. Zero-initialized is the null node.
typedef struct rd_node {
  uint32_t index;
  uint32_t gen;
} rd_node;

typedef void (*rd_destroy_fn)(void* userdata);

static inline bool rd_node_is_null(rd_node n) { return n.index == 0; }
static inline bool rd_node_eq(rd_node a, rd_node b) { return a.index == b.index && a.gen == b.gen; }

rd_ctx*  rd_ctx_create(void);
void     rd_ctx_destroy(rd_ctx* ctx);
rd_node  rd_ctx_root(const rd_ctx* ctx);
uint32_t rd_ctx_node_count(const rd_ctx* ctx);

rd_node rd_node_create(rd_ctx* ctx);
// Destroys the node and its subtree. Destroy callbacks must not mutate the tree.
// During event dispatch the node is detached at once and freed when dispatch ends.
void rd_node_destroy(rd_ctx* ctx, rd_node node);
bool rd_node_alive(const rd_ctx* ctx, rd_node node);

// Moves child if it already has a parent. Fails on cycles, dead handles, or child == root.
bool rd_node_append(rd_ctx* ctx, rd_node parent, rd_node child);
// Null ref appends.
bool rd_node_insert_before(rd_ctx* ctx, rd_node parent, rd_node child, rd_node ref);
void rd_node_detach(rd_ctx* ctx, rd_node node);

rd_node  rd_node_parent(const rd_ctx* ctx, rd_node node);
rd_node  rd_node_first_child(const rd_ctx* ctx, rd_node node);
rd_node  rd_node_last_child(const rd_ctx* ctx, rd_node node);
rd_node  rd_node_next_sibling(const rd_ctx* ctx, rd_node node);
rd_node  rd_node_prev_sibling(const rd_ctx* ctx, rd_node node);
uint32_t rd_node_child_count(const rd_ctx* ctx, rd_node node);

// Replacing userdata runs the previous destroy callback.
// Per-context storage for libraries built on raydom, keyed by the address of something the library
// owns, such as a static variable. destroy(data) runs when the entry is replaced or the context is
// destroyed, after its nodes; it must not call into the context.
bool  rd_ctx_set_extension(rd_ctx* ctx, const void* key, void* data, rd_destroy_fn destroy);
void* rd_ctx_extension(const rd_ctx* ctx, const void* key);

bool  rd_node_set_userdata(rd_ctx* ctx, rd_node node, void* data, rd_destroy_fn destroy);
void* rd_node_userdata(const rd_ctx* ctx, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
