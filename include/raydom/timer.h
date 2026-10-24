#ifndef RAYDOM_TIMER_H
#define RAYDOM_TIMER_H

#include <raydom/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

// Timers run on the context's clock: rd_ctx_set_time, which rd_update calls every frame with raylib's
// time, fires every due timer, in due order, on the calling thread. Callbacks may change the tree and
// set or clear timers; a timer set from a callback doesn't fire before the next clock update. A callback
// that updates the clock itself (rd_ctx_set_time, rd_update) doesn't fire timers there: the run it's in
// fires those that became due once it returns.
typedef struct rd_timer {
  uint32_t index, gen;  // zero is null
} rd_timer;

typedef void (*rd_timer_fn)(rd_ctx* ctx, void* data);

enum { RD_TIMER_REPEAT = 1u << 0 };

// Calls fn after delay seconds, or every delay seconds with RD_TIMER_REPEAT. A repeating timer that
// falls behind skips the missed calls. destroy(data) runs when the timer is cleared, a one-shot timer
// has fired, or the context is destroyed.
rd_timer rd_ctx_set_timer(rd_ctx* ctx, double delay, uint32_t flags, rd_timer_fn fn, void* data, rd_destroy_fn destroy);
// Stops the timer and runs its destroy. False if it was already cleared or, for a one-shot timer, has
// fired, including from its own callback (it's destroyed when that returns). A repeating timer cleared
// from its own callback returns true and doesn't fire again.
bool rd_ctx_clear_timer(rd_ctx* ctx, rd_timer timer);
// Seconds until the next timer is due, from the last clock update: 0 if one is overdue, INFINITY if none.
double rd_ctx_timer_wait(const rd_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif
