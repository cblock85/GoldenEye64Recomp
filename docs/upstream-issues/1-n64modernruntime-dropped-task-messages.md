# [N64ModernRuntime] SP/DP task-completion messages silently dropped when the target queue is full

**Repo:** N64ModernRuntime (`ultramodern/src/mesgqueue.cpp`)

## Summary

`dequeue_external_messages` drops an external message (SP/DP task completion,
VI retrace, …) on the floor when the destination `OSMesgQueue` is full at the
moment of delivery. Games whose schedulers can briefly fill their interrupt
queue then never receive the completion message and wedge.

## Repro

GoldenEye 007 (recompiled via a TLBFREE build of the 100% decomp) wedges within
seconds of gameplay with `pendingGfx == 2`: under a slow renderer, the game's
scheduler interrupt queue fills with retrace messages; the SP/DP-done message
arriving in that window is discarded, so the scheduler waits forever for a task
that already completed.

## Fix that worked

Replace the drop with a FIFO pending queue: drain incoming external messages
into a `std::deque`, then flush into the target queue, stopping at the first
full queue and preserving message order across attempts. Retrying delivery on
the next drain preserves the semantics of the real hardware (where the SP/DP
interrupt would simply still be pending).

```cpp
static std::deque<QueuedMessage> pending_external_messages;

// drain_external_into_pending(): move everything from the thread-safe inbox
// into pending_external_messages (preserving order).
// flush_pending_external(): pop from the front, osSendMesg-ing into each
// message's target queue; stop (and keep the rest pending) at the first
// full target queue.
```

Both the blocking and timed variants of waiting for external messages need to
drain+flush. Happy to submit a PR with the full patch.
