#ifndef RAYDOM_ANIMATE_H
#define RAYDOM_ANIMATE_H

#include <raydom/style.h>
#include <raydom/timer.h>

#ifdef __cplusplus
extern "C" {
#endif

// Transitions: a node whose style has a transition_duration animates its properties from what it
// shows to their new values whenever its style changes: a class added, a state variant applied, an
// inline style set. Animations advance in rd_ctx_layout on the context's clock (rd_ctx_set_time).
//
// Lengths animate when both values have the same unit; height also animates to and from auto.
// Colors, edges, corners and shadows animate. Other properties change at once. A node's first style
// doesn't animate, nor does a node with display: none.

// Animates node from what it shows now to `to`, which is merged into its inline style, over seconds
// with easing, whatever its transition properties say. from, if not null, gives starting values for
// some properties, for animating a new node in. done, if not null, runs after seconds; for example,
// to destroy a node it faded out.
bool rd_node_animate(rd_ctx* ctx, rd_node node, const rd_style* from, const rd_style* to, double seconds,
                     rd_easing easing, rd_timer_fn done, void* data);

// True while any property of any node is animating. rd_set_event_waiting keeps frames running then.
bool rd_ctx_animating(const rd_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif
